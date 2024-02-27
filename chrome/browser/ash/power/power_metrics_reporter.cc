// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/power_metrics_reporter.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal = base::Seconds(60);

}  // namespace

// static
void PowerMetricsReporter::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  metrics::DailyEvent::RegisterPref(registry, prefs::kPowerMetricsDailySample);
}

PowerMetricsReporter::PowerMetricsReporter(
    chromeos::PowerManagerClient* power_manager_client,
    PrefService* local_state_pref_service)
    : power_manager_client_(power_manager_client),
      pref_service_(local_state_pref_service),
      daily_event_(
          std::make_unique<metrics::DailyEvent>(pref_service_,
                                                prefs::kPowerMetricsDailySample,
                                                std::string())) {
  power_manager_client_->AddObserver(this);

  daily_event_->CheckInterval();
  timer_.Start(FROM_HERE, kCheckDailyEventInternal, daily_event_.get(),
               &metrics::DailyEvent::CheckInterval);
}

PowerMetricsReporter::~PowerMetricsReporter() {
  power_manager_client_->RemoveObserver(this);
}

void PowerMetricsReporter::SuspendDone(base::TimeDelta duration) {
  daily_event_->CheckInterval();
}

}  // namespace ash
