// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/stats_reporting_controller.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kPendingPref[] = "pending.cros.metrics.reportingEnabled";

}  // namespace

namespace ash {

static StatsReportingController* g_stats_reporting_controller = nullptr;

// static
void StatsReportingController::Initialize(PrefService* local_state) {
  CHECK(!g_stats_reporting_controller);
  g_stats_reporting_controller = new StatsReportingController(local_state);
}

// static
bool StatsReportingController::IsInitialized() {
  return g_stats_reporting_controller;
}

// static
void StatsReportingController::Shutdown() {
  DCHECK(g_stats_reporting_controller);
  delete g_stats_reporting_controller;
  g_stats_reporting_controller = nullptr;
}

// static
StatsReportingController* StatsReportingController::Get() {
  CHECK(g_stats_reporting_controller);
  return g_stats_reporting_controller;
}

// static
void StatsReportingController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kPendingPref, false,
                                PrefRegistry::NO_REGISTRATION_FLAGS);
}

void StatsReportingController::SetEnabled(Profile* profile, bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Set(profile, base::Value(enabled));
}

bool StatsReportingController::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::Value> value = GetValue();
  return value.has_value() && value->is_bool() && value->GetBool();
}

StatsReportingController::StatsReportingController(PrefService* local_state)
    : OwnerPendingSettingController(kStatsReportingPref,
                                    kPendingPref,
                                    local_state) {
  setting_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kStatsReportingPref,
      base::BindRepeating(&StatsReportingController::NotifyObservers,
                          this->as_weak_ptr()));
}

StatsReportingController::~StatsReportingController() {
  owner_settings_service_observation_.Reset();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace ash
