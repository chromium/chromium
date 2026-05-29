// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/metrics_reporting_level_controller.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kPendingPref[] = "pending.cros.metrics.metricsReportingLevel";

}  // namespace

namespace ash {

static MetricsReportingLevelController* g_metrics_reporting_level_controller =
    nullptr;

// static
void MetricsReportingLevelController::Initialize(PrefService* local_state) {
  CHECK(!g_metrics_reporting_level_controller);
  g_metrics_reporting_level_controller =
      new MetricsReportingLevelController(local_state);
}

// static
bool MetricsReportingLevelController::IsInitialized() {
  return g_metrics_reporting_level_controller;
}

// static
void MetricsReportingLevelController::Shutdown() {
  CHECK(g_metrics_reporting_level_controller);
  delete g_metrics_reporting_level_controller;
  g_metrics_reporting_level_controller = nullptr;
}

// static
MetricsReportingLevelController* MetricsReportingLevelController::Get() {
  CHECK(g_metrics_reporting_level_controller);
  return g_metrics_reporting_level_controller;
}

// static
void MetricsReportingLevelController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kPendingPref, static_cast<int>(metrics::MetricsReportingLevel::kNone),
      PrefRegistry::NO_REGISTRATION_FLAGS);
}

void MetricsReportingLevelController::SetLevel(
    Profile* profile,
    metrics::MetricsReportingLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Set(profile, base::Value(static_cast<int>(level)));
}

metrics::MetricsReportingLevel MetricsReportingLevelController::GetLevel()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::Value> value = GetValue();
  if (!value.has_value() || !value->is_int()) {
    return metrics::MetricsReportingLevel::kNone;
  }
  return static_cast<metrics::MetricsReportingLevel>(value->GetInt());
}

MetricsReportingLevelController::MetricsReportingLevelController(
    PrefService* local_state)
    : OwnerPendingSettingController(kMetricsReportingLevelPref,
                                    kPendingPref,
                                    local_state) {
  setting_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kMetricsReportingLevelPref,
      base::BindRepeating(&MetricsReportingLevelController::NotifyObservers,
                          this->as_weak_ptr()));
}

MetricsReportingLevelController::~MetricsReportingLevelController() = default;

}  // namespace ash
