// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/metrics_reporter.h"

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal = base::Seconds(60);

// Prefs corresponding to DeviceClass values.
constexpr std::array<const char*, MetricsReporter::kNumberDeviceClasses>
    kDailyCountPrefs = {
        prefs::kAutoScreenBrightnessMetricsNoAlsUserAdjustmentCount,
        prefs::kAutoScreenBrightnessMetricsSupportedAlsUserAdjustmentCount,
        prefs::kAutoScreenBrightnessMetricsUnsupportedAlsUserAdjustmentCount,
        prefs::kAutoScreenBrightnessMetricsAtlasUserAdjustmentCount,
        prefs::kAutoScreenBrightnessMetricsEveUserAdjustmentCount,
        prefs::kAutoScreenBrightnessMetricsNocturneUserAdjustmentCount,
        prefs::kAutoScreenBrightnessMetricsKohakuUserAdjustmentCount,
};

// Histograms corresponding to UserAdjustment values.
constexpr std::array<const char*, MetricsReporter::kNumberDeviceClasses>
    kDailyCountHistograms = {
        MetricsReporter::kNoAlsUserAdjustmentName,
        MetricsReporter::kSupportedAlsUserAdjustmentName,
        MetricsReporter::kUnsupportedAlsUserAdjustmentName,
        MetricsReporter::kAtlasUserAdjustmentName,
        MetricsReporter::kEveUserAdjustmentName,
        MetricsReporter::kNocturneUserAdjustmentName,
        MetricsReporter::kKohakuUserAdjustmentName,
};

}  // namespace

constexpr char MetricsReporter::kDailyEventIntervalName[];
constexpr char MetricsReporter::kNoAlsUserAdjustmentName[];
constexpr char MetricsReporter::kSupportedAlsUserAdjustmentName[];
constexpr char MetricsReporter::kUnsupportedAlsUserAdjustmentName[];
constexpr char MetricsReporter::kAtlasUserAdjustmentName[];
constexpr char MetricsReporter::kEveUserAdjustmentName[];
constexpr char MetricsReporter::kNocturneUserAdjustmentName[];
constexpr char MetricsReporter::kKohakuUserAdjustmentName[];

constexpr int MetricsReporter::kNumberDeviceClasses;

// This class is needed since metrics::DailyEvent requires taking ownership
// of its observers. It just forwards events to MetricsReporter.
class MetricsReporter::DailyEventObserver
    : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(MetricsReporter* reporter)
      : reporter_(reporter) {}

  DailyEventObserver(const DailyEventObserver&) = delete;
  DailyEventObserver& operator=(const DailyEventObserver&) = delete;

  ~DailyEventObserver() override = default;

  // metrics::DailyEvent::Observer:
  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    reporter_->ReportDailyMetrics(type);
  }

 private:
  raw_ptr<MetricsReporter> reporter_;  // Not owned.
};

void MetricsReporter::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(
      registry, prefs::kAutoScreenBrightnessMetricsDailySample);
  for (const char* daily_count_pref : kDailyCountPrefs) {
    registry->RegisterIntegerPref(daily_count_pref, 0);
  }
}

MetricsReporter::MetricsReporter(
    chromeos::PowerManagerClient* power_manager_client,
    PrefService* local_state_pref_service)
    : pref_service_(local_state_pref_service),
      daily_event_(std::make_unique<metrics::DailyEvent>(
          pref_service_,
          prefs::kAutoScreenBrightnessMetricsDailySample,
          kDailyEventIntervalName)) {
  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = pref_service_->GetInteger(kDailyCountPrefs[i]);
  }

  power_manager_client_observation_.Observe(power_manager_client);

  daily_event_->AddObserver(std::make_unique<DailyEventObserver>(this));
  daily_event_->CheckInterval();
  timer_.Start(FROM_HERE, kCheckDailyEventInternal, daily_event_.get(),
               &metrics::DailyEvent::CheckInterval);
}

MetricsReporter::~MetricsReporter() = default;

void MetricsReporter::SuspendDone(base::TimeDelta duration) {
  daily_event_->CheckInterval();
}

void MetricsReporter::SetDeviceClass(DeviceClass device_class) {
  DCHECK(!device_class_);
  device_class_ = device_class;
  DCHECK_LT(static_cast<size_t>(device_class), kDailyCountPrefs.size());
}

void MetricsReporter::OnUserBrightnessChangeRequested() {
  DCHECK(device_class_);
  const size_t index = static_cast<size_t>(*device_class_);
  const char* daily_count_pref = kDailyCountPrefs[index];
  ++daily_counts_[index];
  pref_service_->SetInteger(daily_count_pref, daily_counts_[index]);
}

void MetricsReporter::ReportDailyMetricsForTesting(
    metrics::DailyEvent::IntervalType type) {
  ReportDailyMetrics(type);
}

void MetricsReporter::ReportDailyMetrics(
    metrics::DailyEvent::IntervalType type) {
  if (!device_class_)
    return;

  // Don't send metrics on first run or if the clock is changed.
  if (type == metrics::DailyEvent::IntervalType::DAY_ELAPSED) {
    const size_t index = static_cast<size_t>(*device_class_);
    base::UmaHistogramCounts100(kDailyCountHistograms[index],
                                daily_counts_[index]);
  }

  for (size_t i = 0; i < kDailyCountPrefs.size(); ++i) {
    daily_counts_[i] = 0;
    pref_service_->SetInteger(kDailyCountPrefs[i], 0);
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
