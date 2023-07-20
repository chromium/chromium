// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/update_notification_showing_controller.h"

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/version.h"
#include "chrome/browser/ash/notifications/update_notification.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {
namespace {

// Target milestone scope for the `UpdateNotification`.
constexpr int kMilestoneScope[] = {118, 119, 120};

int CurrentMilestone() {
  return version_info::GetVersion().components()[0];
}

int LastShownMilestone(Profile* profile) {
  int last_shown_milestone = profile->GetPrefs()->GetInteger(
      prefs::kUpdateNotificationLastShownMilestone);
  if (profile->GetPrefs()
          ->FindPreference(prefs::kUpdateNotificationLastShownMilestone)
          ->IsDefaultValue()) {
    // We don't know if the user has seen any notification before as we have
    // never set which milestone was last seen. So use the version of chrome
    // where the profile was created instead.
    base::Version profile_version(
        ChromeVersionService::GetVersion(profile->GetPrefs()));
    last_shown_milestone = profile_version.components()[0];
  }
  return last_shown_milestone;
}

bool IsEligibleProfile(Profile* profile) {
  // Do not show the notification for Ephemeral and Guest profiles.
  if (ProfileHelper::IsEphemeralUserProfile(profile)) {
    return false;
  }
  if (profile->IsGuestSession()) {
    return false;
  }

  // Show the notification for Googlers.
  if (gaia::IsGoogleInternalAccountEmail(profile->GetProfileUserName())) {
    return true;
  }

  // Show the notification for Unicorn profiles. Education profiles are Regular
  // profiles, so they will not pass this check.
  if (profile->IsChild()) {
    return true;
  }

  // After the above exceptions, do not show the notification for profiles
  // managed by a policy.
  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    return false;
  }

  // Otherwise, show the notification for Consumer profiles.
  return ProfileHelper::Get()->GetUserByProfile(profile)->HasGaiaAccount();
}

}  // namespace

void UpdateNotificationShowingController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // Initiates the last seen milestone with a negative number.
  registry->RegisterIntegerPref(prefs::kUpdateNotificationLastShownMilestone,
                                -10);
}

UpdateNotificationShowingController::UpdateNotificationShowingController(
    Profile* profile)
    : profile_(profile) {
  if (!profile_) {
    CHECK_IS_TEST();
  }
  current_milestone_ = CurrentMilestone();
}

UpdateNotificationShowingController::~UpdateNotificationShowingController() =
    default;

void UpdateNotificationShowingController::MaybeShowUpdateNotification() {
  // Not show the notification if the current milestone is not in the
  // target milestone scope.
  if (!base::Contains(kMilestoneScope, current_milestone_)) {
    return;
  }

  // Not show the notification if the preference is not generated.
  if (!profile_->GetPrefs()) {
    return;
  }

  // Not show the notification if the notification is already shown in
  // any target milestone.
  if (base::Contains(kMilestoneScope, LastShownMilestone(profile_))) {
    return;
  }

  // Not show the notification for Ephemeral user,  guest users, non-googler
  // managed profiles (eg. enterprise, education) and etc.
  if (!IsEligibleProfile(profile_)) {
    return;
  }

  if (!update_notification_) {
    update_notification_ = std::make_unique<UpdateNotification>(profile_, this);
  }
  update_notification_->ShowNotification();
}

void UpdateNotificationShowingController::MarkNotificationShown() {
  // TODO(b/284978852): Add UMA tracking.
  profile_->GetPrefs()->SetInteger(prefs::kUpdateNotificationLastShownMilestone,
                                   current_milestone_);
}

void UpdateNotificationShowingController::SetFakeCurrentMilestoneForTesting(
    int fake_milestone) {
  current_milestone_ = fake_milestone;
}

}  // namespace ash
