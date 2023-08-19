// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/privacy_hub/privacy_hub_hats_trigger.h"

#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {
namespace {
const HatsConfig& kHatsConfig = kPrivacyHubPostLaunchSurvey;
constexpr base::TimeDelta kShowSurveyDelay = base::Seconds(40);
}

PrivacyHubHatsTrigger& PrivacyHubHatsTrigger::Get() {
  static base::NoDestructor<PrivacyHubHatsTrigger> instance;
  return *instance;
}

void PrivacyHubHatsTrigger::ShowSurveyAfterDelayElapsed() {
  // The user has already seen a survey or we're about to show them one.
  if (hats_controller_ || show_notification_timer_.IsRunning()) {
    return;
  }

  show_notification_timer_.Start(
      FROM_HERE, kShowSurveyDelay,
      base::BindOnce(&PrivacyHubHatsTrigger::ShowSurveyIfSelected,
                     base::Unretained(this)));
}

PrivacyHubHatsTrigger::PrivacyHubHatsTrigger() = default;
PrivacyHubHatsTrigger::~PrivacyHubHatsTrigger() = default;

void PrivacyHubHatsTrigger::ShowSurveyIfSelected() {
  // We only show the survey if the current session is still active.
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return;
  }

  Profile* profile = GetProfile();
  if (!profile) {
    // This can happen in tests when there is no `ProfileManager` instance.
    return;
  }

  if (HatsNotificationController::ShouldShowSurveyToProfile(profile,
                                                            kHatsConfig)) {
    hats_controller_ =
        base::MakeRefCounted<HatsNotificationController>(profile, kHatsConfig);
  }
}

void PrivacyHubHatsTrigger::SetNoProfileForTesting(const bool no_profile) {
  no_profile_for_testing_ = no_profile;
}

const HatsNotificationController*
PrivacyHubHatsTrigger::GetHatsNotificationControllerForTesting() const {
  return hats_controller_.get();
}

base::OneShotTimer& PrivacyHubHatsTrigger::GetTimerForTesting() {
  return show_notification_timer_;
}

Profile* PrivacyHubHatsTrigger::GetProfile() const {
  if (no_profile_for_testing_) {
    return nullptr;
  }

  return ProfileManager::GetActiveUserProfile();
}

}  // namespace ash
