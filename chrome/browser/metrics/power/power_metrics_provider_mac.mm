// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_provider_mac.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/power_metrics/smc_mac.h"

namespace {
constexpr base::TimeDelta kStartupPowerMetricsCollectionDuration =
    base::Seconds(30);
constexpr base::TimeDelta kStartupPowerMetricsCollectionInterval =
    base::Seconds(1);
constexpr base::TimeDelta kPostStartupPowerMetricsCollectionInterval =
    base::Seconds(60);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ThermalStateUMA {
  kNominal = 0,
  kFair = 1,
  kSerious = 2,
  kCritical = 3,
  kMaxValue = kCritical,
};

ThermalStateUMA ThermalStateToUmaEnumValue(NSProcessInfoThermalState state) {
  switch (state) {
    case NSProcessInfoThermalStateNominal:
      return ThermalStateUMA::kNominal;
    case NSProcessInfoThermalStateFair:
      return ThermalStateUMA::kFair;
    case NSProcessInfoThermalStateSerious:
      return ThermalStateUMA::kSerious;
    case NSProcessInfoThermalStateCritical:
      return ThermalStateUMA::kCritical;
  }
}

void RecordSMCHistogram(std::string_view prefix,
                        std::string_view suffix,
                        std::optional<double> watts) {
  if (watts.has_value()) {
    double milliwatts = watts.value() * 1000;
    base::UmaHistogramCounts100000(base::StrCat({prefix, suffix}), milliwatts);
  }
}

}  // namespace

class PowerMetricsProvider::Impl {
 public:
  Impl() : smc_reader_(power_metrics::SMCReader::Create()) {
    ScheduleCollection();
  }

  ~Impl() = default;

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

 private:
  bool IsInStartup() {
    if (could_be_in_startup_) {
      const base::TimeDelta process_uptime =
          base::Time::Now() - base::Process::Current().CreationTime();
      if (process_uptime >= kStartupPowerMetricsCollectionDuration)
        could_be_in_startup_ = false;
    }
    return could_be_in_startup_;
  }

  void ScheduleCollection() {
    timer_.Start(FROM_HERE,
                 IsInStartup() ? kStartupPowerMetricsCollectionInterval
                               : kPostStartupPowerMetricsCollectionInterval,
                 this, &Impl::Collect);
  }

  void Collect() {
    ScheduleCollection();

    if (IsInStartup()) {
      RecordSMC("DuringStartup");
    } else {
      RecordSMC("All");
      RecordThermal();
    }
  }

  void RecordSMC(std::string_view suffix) {
    if (!smc_reader_)
      return;

    RecordSMCHistogram("Power.Mac.Total.", suffix,
                       smc_reader_->ReadKey(SMCKeyIdentifier::TotalPower));
    RecordSMCHistogram("Power.Mac.CPU.", suffix,
                       smc_reader_->ReadKey(SMCKeyIdentifier::CPUPower));
    RecordSMCHistogram("Power.Mac.GPUi.", suffix,
                       smc_reader_->ReadKey(SMCKeyIdentifier::iGPUPower));
    RecordSMCHistogram("Power.Mac.GPU0.", suffix,
                       smc_reader_->ReadKey(SMCKeyIdentifier::GPU0Power));
    RecordSMCHistogram("Power.Mac.GPU1.", suffix,
                       smc_reader_->ReadKey(SMCKeyIdentifier::GPU1Power));
  }

  void RecordThermal() {
    UMA_HISTOGRAM_ENUMERATION(
        "Power.Mac.ThermalState",
        ThermalStateToUmaEnumValue([[NSProcessInfo processInfo] thermalState]));
  }

  base::OneShotTimer timer_;
  std::unique_ptr<power_metrics::SMCReader> smc_reader_;
  bool could_be_in_startup_ = true;
};

PowerMetricsProvider::PowerMetricsProvider() = default;
PowerMetricsProvider::~PowerMetricsProvider() = default;

void PowerMetricsProvider::OnRecordingEnabled() {
  impl_ = base::SequenceBound<Impl>(base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
}

void PowerMetricsProvider::OnRecordingDisabled() {
  impl_.reset();
}
