// SPDX-License-Identifier: GPL-2.0+

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QSignalBlocker>
#include <QCoreApplication>

#include <cmath>

#include "HorizontalDock.h"
#include "dockwindows.h"

#include "scopesettings.h"
#include "sispinbox.h"
#include "utils/printutils.h"

template<typename... Args> struct SELECT {
    template<typename C, typename R>
    static constexpr auto OVERLOAD_OF( R (C::*pmf)(Args...) ) -> decltype(pmf) {
        return pmf;
    }
};

HorizontalDock::HorizontalDock(DsoSettingsScope *scope, QWidget *parent, Qt::WindowFlags flags)
    : QDockWidget(tr("Horizontal"), parent, flags), scope(scope) {

    // Initialize elements
    this->samplerateLabel = new QLabel(tr("Samplerate"));
    this->samplerateSiSpinBox = new SiSpinBox(UNIT_SAMPLES);
    this->samplerateSiSpinBox->setMinimum(1);
    this->samplerateSiSpinBox->setMaximum(1e8);
    this->samplerateSiSpinBox->setUnitPostfix("/s");

    timebaseSteps << 1.0 << 2.0 << 4.0 << 10.0;

    this->timebaseLabel = new QLabel(tr("Timebase"));
    this->timebaseSiSpinBox = new SiSpinBox(UNIT_SECONDS);
    this->timebaseSiSpinBox->setSteps(timebaseSteps);
    this->timebaseSiSpinBox->setMinimum(1e-9);
    this->timebaseSiSpinBox->setMaximum(3.6e3);

    this->frequencybaseLabel = new QLabel(tr("Frequencybase"));
    this->frequencybaseSiSpinBox = new SiSpinBox(UNIT_HERTZ);
    this->frequencybaseSiSpinBox->setMinimum(1.0);
    this->frequencybaseSiSpinBox->setMaximum(100e6);

    this->recordLengthLabel = new QLabel(tr("Record length"));
    this->recordLengthComboBox = new QComboBox();

    this->formatLabel = new QLabel(tr("Format"));
    this->formatComboBox = new QComboBox();
    for (Dso::GraphFormat format: Dso::GraphFormatEnum)
        this->formatComboBox->addItem(Dso::graphFormatString(format));

    this->dockLayout = new QGridLayout();
    this->dockLayout->setColumnMinimumWidth(0, 64);
    this->dockLayout->setColumnStretch(1, 1);
    this->dockLayout->addWidget(this->samplerateLabel, 0, 0);
    this->dockLayout->addWidget(this->samplerateSiSpinBox, 0, 1);
    this->dockLayout->addWidget(this->timebaseLabel, 1, 0);
    this->dockLayout->addWidget(this->timebaseSiSpinBox, 1, 1);
    this->dockLayout->addWidget(this->frequencybaseLabel, 2, 0);
    this->dockLayout->addWidget(this->frequencybaseSiSpinBox, 2, 1);
    this->dockLayout->addWidget(this->recordLengthLabel, 3, 0);
    this->dockLayout->addWidget(this->recordLengthComboBox, 3, 1);
    this->dockLayout->addWidget(this->formatLabel, 4, 0);
    this->dockLayout->addWidget(this->formatComboBox, 4, 1);

    this->dockWidget = new QWidget();
    SetupDockWidget(this, dockWidget, dockLayout);

    // Connect signals and slots
    connect(this->samplerateSiSpinBox, SELECT<double>::OVERLOAD_OF(&QDoubleSpinBox::valueChanged), this, &HorizontalDock::samplerateSelected);
    connect(this->timebaseSiSpinBox, SELECT<double>::OVERLOAD_OF(&QDoubleSpinBox::valueChanged), this, &HorizontalDock::timebaseSelected);
    connect(this->frequencybaseSiSpinBox, SELECT<double>::OVERLOAD_OF(&QDoubleSpinBox::valueChanged), this, &HorizontalDock::frequencybaseSelected);
    connect(this->recordLengthComboBox, SELECT<int>::OVERLOAD_OF(&QComboBox::currentIndexChanged), this, &HorizontalDock::recordLengthSelected);
    connect(this->formatComboBox, SELECT<int>::OVERLOAD_OF(&QComboBox::currentIndexChanged), this, &HorizontalDock::formatSelected);

    // Set values
    this->setSamplerate(scope->horizontal.samplerate);
    this->setTimebase(scope->horizontal.timebase);
    this->setFrequencybase(scope->horizontal.frequencybase);
    // this->setRecordLength(scope->horizontal.recordLength);
    this->setFormat(scope->horizontal.format);
}

/// \brief Don't close the dock, just hide it.
/// \param event The close event that should be handled.
void HorizontalDock::closeEvent(QCloseEvent *event) {
    this->hide();

    event->accept();
}

void HorizontalDock::setFrequencybase(double frequencybase) {
    QSignalBlocker blocker(frequencybaseSiSpinBox);
    frequencybaseSiSpinBox->setValue(frequencybase);
}

void HorizontalDock::setSamplerate(double samplerate) {
    QSignalBlocker blocker(samplerateSiSpinBox);
    samplerateSiSpinBox->setValue(samplerate);
}

double HorizontalDock::setTimebase(double timebase) {
    QSignalBlocker blocker(timebaseSiSpinBox);
    // timebaseSteps are repeated in each decade
    double decade = pow(10, floor(log10(timebase)));
    double vNorm = timebase / decade;
    for (int i = 0; i < timebaseSteps.size() - 1; ++i) {
        if (timebaseSteps.at(i) <= vNorm && vNorm < timebaseSteps.at(i + 1)) {
            timebaseSiSpinBox->setValue(decade * timebaseSteps.at(i));
            break;
        }
    }
    return timebaseSiSpinBox->value();
}

int addRecordLength(QComboBox *recordLengthComboBox, unsigned recordLength) {
    recordLengthComboBox->addItem(
        recordLength == UINT_MAX ? QCoreApplication::translate("HorizontalDock","Roll") : valueToString(recordLength, UNIT_SAMPLES, 3), recordLength);
    return recordLengthComboBox->count()-1;
}

void HorizontalDock::setRecordLength(unsigned int recordLength) {
    QSignalBlocker blocker(recordLengthComboBox);
    int index = recordLengthComboBox->findData(recordLength);
    scope->horizontal.recordLength = recordLength;

    if (index == -1) {
        index = addRecordLength(recordLengthComboBox, recordLength);
    }
    recordLengthComboBox->setCurrentIndex(index);
}

int HorizontalDock::setFormat(Dso::GraphFormat format) {
    QSignalBlocker blocker(formatComboBox);
    if (format >= Dso::GraphFormat::TY && format <= Dso::GraphFormat::XY) {
        formatComboBox->setCurrentIndex(format);
        return format;
    }

    return -1;
}

void HorizontalDock::setAvailableRecordLengths(const std::vector<unsigned> &recordLengths) {
    QSignalBlocker blocker(recordLengthComboBox);

    recordLengthComboBox->clear();
    for (auto recordLength : recordLengths) {
        addRecordLength(recordLengthComboBox, recordLength);
    }

    setRecordLength(scope->horizontal.recordLength);
}

void HorizontalDock::setSamplerateLimits(double minimum, double maximum) {
    QSignalBlocker blocker(samplerateSiSpinBox);
    this->samplerateSiSpinBox->setMinimum(minimum);
    this->samplerateSiSpinBox->setMaximum(maximum);
}

void HorizontalDock::setSamplerateSteps(int mode, QList<double> steps) {
    // Assume that method is invoked for fixed samplerate devices only
    QSignalBlocker samplerateBlocker(samplerateSiSpinBox);
    samplerateSiSpinBox->setMode(mode);
    samplerateSiSpinBox->setSteps(steps);
    samplerateSiSpinBox->setMinimum(steps.first());
    samplerateSiSpinBox->setMaximum(steps.last());
    // Make reasonable adjustments to the timebase spinbox
    QSignalBlocker timebaseBlocker(timebaseSiSpinBox);
    timebaseSiSpinBox->setMinimum(pow(10, floor(log10(1.0 / steps.last()))));
    timebaseSiSpinBox->setMaximum(pow(10, ceil(log10(8192.0 / (steps.first() * 10)))));
}

/// \brief Called when the frequencybase spinbox changes its value.
/// \param frequencybase The frequencybase in hertz.
void HorizontalDock::frequencybaseSelected(double frequencybase) {
    scope->horizontal.frequencybase = frequencybase;
    emit frequencybaseChanged(frequencybase);
}

/// \brief Called when the samplerate spinbox changes its value.
/// \param samplerate The samplerate in samples/second.
void HorizontalDock::samplerateSelected(double samplerate) {
    scope->horizontal.samplerate = samplerate;
    scope->horizontal.samplerateSource = DsoSettingsScopeHorizontal::Samplerrate;
    emit samplerateChanged(samplerate);
}

/// \brief Called when the timebase spinbox changes its value.
/// \param timebase The timebase in seconds.
void HorizontalDock::timebaseSelected(double timebase) {
    scope->horizontal.timebase = timebase;
    scope->horizontal.samplerateSource = DsoSettingsScopeHorizontal::Duration;
    emit timebaseChanged(timebase);
}

/// \brief Called when the record length combo box changes its value.
/// \param index The index of the combo box item.
void HorizontalDock::recordLengthSelected(int index) {
    scope->horizontal.recordLength = this->recordLengthComboBox->itemData(index).toUInt();
    emit recordLengthChanged(index);
}

/// \brief Called when the format combo box changes its value.
/// \param index The index of the combo box item.
void HorizontalDock::formatSelected(int index) {
    scope->horizontal.format = (Dso::GraphFormat)index;
    emit formatChanged(scope->horizontal.format);
}
