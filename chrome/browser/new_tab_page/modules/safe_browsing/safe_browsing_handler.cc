// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/safe_browsing/safe_browsing_handler.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/safe_browsing/safe_browsing_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search/ntp_features.h"

using ::safe_browsing::IsEnhancedProtectionEnabled;
using ::safe_browsing::SafeBrowsingMetricsCollectorFactory;
using ::safe_browsing::SafeBrowsingPolicyHandler;

namespace ntp {
namespace {
// Get end time for last cooldown. Returns epoch + cooldown_period if no
// cooldown happened.
base::Time GetLastCooldownEndTime(PrefService* pref_service) {
  base::Time cooldown_start = base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(pref_service->GetInt64(
          prefs::kSafeBrowsingModuleLastCooldownStartAt)));
  // The field param is passed as a double, to allow manual testing using a
  // fraction of a day as cooldown.
  double cooldown_days = base::GetFieldTrialParamByFeatureAsDouble(
      ntp_features::kNtpSafeBrowsingModule,
      ntp_features::kNtpSafeBrowsingModuleCooldownPeriodDaysParam,
      /*default_value=*/30.0);
  base::TimeDelta cooldown_period = base::Seconds(static_cast<int>(
      cooldown_days * base::Time::kHoursPerDay * base::Time::kSecondsPerHour));
  return cooldown_start + cooldown_period;
}

void SetModuleCooldownPrefs(PrefService* pref_service,
                            int64_t cooldown_start_time,
                            int module_shown_count) {
  pref_service->SetInt64(prefs::kSafeBrowsingModuleLastCooldownStartAt,
                         cooldown_start_time);
  pref_service->SetInteger(prefs::kSafeBrowsingModuleShownCount,
                           module_shown_count);
}
}  // namespace

SafeBrowsingHandler::SafeBrowsingHandler(
    mojo::PendingReceiver<ntp::safe_browsing::mojom::SafeBrowsingHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      metrics_collector_(
          SafeBrowsingMetricsCollectorFactory::GetForProfile(profile)),
      pref_service_(profile->GetPrefs()),
      saved_last_cooldown_start_time_(0),
      saved_module_shown_count_(0) {}

SafeBrowsingHandler::~SafeBrowsingHandler() = default;

void SafeBrowsingHandler::CanShowModule(CanShowModuleCallback callback) {
  bool managed =
      SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
          pref_service_);
  bool already_enabled = IsEnhancedProtectionEnabled(*pref_service_);
  bool module_already_opened =
      pref_service_->GetBoolean(prefs::kSafeBrowsingModuleOpened);
  base::Time cooldown_end = GetLastCooldownEndTime(pref_service_);

  // Do not show the module if Safe Browsing protection level is controlled by
  // policy, or is already enabled, if the user has clicked on the module button
  // earlier, or if the user is in cooldown.
  if (managed || already_enabled || module_already_opened ||
      (base::Time::Now() < cooldown_end)) {
    std::move(callback).Run(false);
    return;
  }

  std::optional<base::Time> latest_event_time =
      metrics_collector_->GetLatestSecuritySensitiveEventTimestamp();
  // Do not show if there is no security sensitive event after the latest
  // cooldown.
  if (!latest_event_time.has_value() || latest_event_time < cooldown_end) {
    std::move(callback).Run(false);
    return;
  }

  int module_shown_count =
      pref_service_->GetInteger(prefs::kSafeBrowsingModuleShownCount);
  int module_shown_count_max = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpSafeBrowsingModule,
      ntp_features::kNtpSafeBrowsingModuleCountMaxParam,
      /*default_value=*/5);

  // Start cooldown if module has been shown module_shown_count_max times.
  if (module_shown_count + 1 >= module_shown_count_max) {
    SetModuleCooldownPrefs(
        pref_service_, base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
        /*module_shown_count=*/0);
  } else {
    pref_service_->SetInteger(prefs::kSafeBrowsingModuleShownCount,
                              module_shown_count + 1);
  }
  std::move(callback).Run(true);
}

void SafeBrowsingHandler::ProcessModuleClick() {
  pref_service_->SetBoolean(prefs::kSafeBrowsingModuleOpened, true);
}

void SafeBrowsingHandler::DismissModule() {
  saved_last_cooldown_start_time_ =
      pref_service_->GetInt64(prefs::kSafeBrowsingModuleLastCooldownStartAt);
  saved_module_shown_count_ =
      pref_service_->GetInteger(prefs::kSafeBrowsingModuleShownCount);
  SetModuleCooldownPrefs(
      pref_service_, base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
      /*module_shown_count=*/0);
}

void SafeBrowsingHandler::RestoreModule() {
  SetModuleCooldownPrefs(pref_service_, saved_last_cooldown_start_time_,
                         saved_module_shown_count_);
}

// static
void SafeBrowsingHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kSafeBrowsingModuleShownCount, 0);
  registry->RegisterInt64Pref(prefs::kSafeBrowsingModuleLastCooldownStartAt, 0);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingModuleOpened, false);
}

}  // namespace ntp
