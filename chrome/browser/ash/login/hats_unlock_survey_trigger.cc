// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/hats_unlock_survey_trigger.h"

#include "ash/public/cpp/tablet_mode.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_service.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

namespace {

std::string AuthMethodToString(HatsUnlockSurveyTrigger::AuthMethod method) {
  switch (method) {
    case HatsUnlockSurveyTrigger::AuthMethod::kPassword:
      return "password";
    case HatsUnlockSurveyTrigger::AuthMethod::kPin:
      return "pin";
    case HatsUnlockSurveyTrigger::AuthMethod::kSmartlock:
      return "smartlock";
    case HatsUnlockSurveyTrigger::AuthMethod::kFingerprint:
      return "fingerprint";
    case HatsUnlockSurveyTrigger::AuthMethod::kChallengeResponse:
      return "challengeresponse";
    case HatsUnlockSurveyTrigger::AuthMethod::kNothing:
      return "nothing";
  }
}

bool IsSessionLocked() {
  return session_manager::SessionManager::Get()->session_state() ==
         session_manager::SessionState::LOCKED;
}

}  // namespace

HatsUnlockSurveyTrigger::Impl::Impl() = default;

HatsUnlockSurveyTrigger::Impl::~Impl() = default;

bool HatsUnlockSurveyTrigger::Impl::ShouldShowSurveyToProfile(
    Profile* profile,
    const HatsConfig& hats_config) {
  // The survey has already been shown and should not be shown again.
  if (hats_notification_controller_) {
    return false;
  }

  return HatsNotificationController::ShouldShowSurveyToProfile(profile,
                                                               hats_config);
}

void HatsUnlockSurveyTrigger::Impl::ShowSurvey(
    Profile* profile,
    const HatsConfig& hats_config,
    const base::flat_map<std::string, std::string>& product_specific_data) {
  hats_notification_controller_ =
      base::MakeRefCounted<HatsNotificationController>(profile, hats_config,
                                                       product_specific_data);
}

HatsUnlockSurveyTrigger::HatsUnlockSurveyTrigger()
    : HatsUnlockSurveyTrigger(std::make_unique<Impl>()) {}

HatsUnlockSurveyTrigger::HatsUnlockSurveyTrigger(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

HatsUnlockSurveyTrigger::~HatsUnlockSurveyTrigger() = default;

void HatsUnlockSurveyTrigger::ShowSurveyIfSelected(const AccountId& account_id,
                                                   AuthMethod method) {
  DCHECK_NE(method, AuthMethod::kNothing);

  // Survey is only shown for unlock, not signin, so the session is required to
  // be locked.
  if (!IsSessionLocked() || !account_id.is_valid()) {
    return;
  }

  Profile* profile = GetProfile(account_id);
  // If the profile isn't loaded, the survey is simply not shown. This isn't
  // expected to happen since the profile should have been loaded during
  // signin.
  if (!profile) {
    return;
  }

  // A different config is used for Smart Lock because we want to use a higher
  // sample rate.
  const HatsConfig& hats_config = method == AuthMethod::kSmartlock
                                      ? kHatsSmartLockSurvey
                                      : kHatsUnlockSurvey;

  // Checks prefs to make sure a survey hasn't already been shown to the user
  // this survey cycle, and rolls a die to determine if the survey should be
  // shown.
  if (!impl_->ShouldShowSurveyToProfile(profile, hats_config)) {
    return;
  }

  SmartLockService* smart_lock_service =
      SmartLockServiceFactory::GetForBrowserContext(profile);
  const bool smartlock_enabled =
      smart_lock_service && smart_lock_service->IsEnabled();
  const std::string smartlock_remotestatus =
      smart_lock_service
          ? smart_lock_service->GetLastRemoteStatusUnlockForLogging()
          : std::string();

  base::flat_map<std::string, std::string> product_specific_data = {
      {"authMethod", AuthMethodToString(method)},
      {"tabletMode", TabletMode::Get()->InTabletMode() ? "true" : "false"},
      {"smartLockEnabled", smartlock_enabled ? "true" : "false"},
      {"smartLockGetRemoteStatusUnlock", smartlock_remotestatus}};

  impl_->ShowSurvey(profile, hats_config, product_specific_data);
}

void HatsUnlockSurveyTrigger::SetProfileForTesting(Profile* profile) {
  profile_for_testing_ = profile;
}

Profile* HatsUnlockSurveyTrigger::GetProfile(const AccountId& account_id) {
  if (profile_for_testing_) {
    return profile_for_testing_;
  }

  return ProfileHelper::Get()->GetProfileByAccountId(account_id);
}

}  // namespace ash
