// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/release_notes/release_notes_storage.h"

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/version.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

constexpr int kTimesToShowSuggestionChip = 3;

int GetMilestone() {
  return version_info::GetVersion().components()[0];
}

bool IsEligibleProfile(Profile* profile) {
  // Do not show the notification for Ephemeral and Guest profiles.
  if (ash::ProfileHelper::IsEphemeralUserProfile(profile))
    return false;
  if (profile->IsGuestSession())
    return false;

  // Do not show the notification for managed profiles (e.g. Enterprise,
  // Education), except for Googlers and Unicorn accounts.

  // Show the notification for Googlers.
  if (gaia::IsGoogleInternalAccountEmail(profile->GetProfileUserName()))
    return true;

  // Show the notification for Unicorn profiles. Education profiles are Regular
  // profiles, so they will not pass this check.
  if (profile->IsChild())
    return true;

  // After the above exceptions, do not show the notification for profiles
  // managed by a policy.
  if (profile->GetProfilePolicyConnector()->IsManaged())
    return false;

  // Otherwise, show the notification for Consumer profiles.
  return ash::ProfileHelper::Get()->GetUserByProfile(profile)->HasGaiaAccount();
}

bool ShouldShowForCurrentChannel() {
  return chrome::GetChannel() == version_info::Channel::STABLE ||
         base::FeatureList::IsEnabled(
             ash::features::kReleaseNotesNotificationAllChannels);
}

}  // namespace

namespace ash {

// Called on every session startup.
void ReleaseNotesStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);
}

ReleaseNotesStorage::ReleaseNotesStorage(Profile* profile)
    : profile_(profile) {}

ReleaseNotesStorage::~ReleaseNotesStorage() = default;

bool ReleaseNotesStorage::ShouldNotify() {
  // TODO(b/174514401): Make this server controlled.
  if (base::FeatureList::IsEnabled(
          ash::features::kReleaseNotesNotificationAlwaysEligible)) {
    return true;
  }

  if (!ShouldShowForCurrentChannel()) {
    return false;
  }

  if (!IsEligibleProfile(profile_))
    return false;

  int last_milestone = profile_->GetPrefs()->GetInteger(
      prefs::kHelpAppNotificationLastShownMilestone);
  if (profile_->GetPrefs()
          ->FindPreference(prefs::kHelpAppNotificationLastShownMilestone)
          ->IsDefaultValue()) {
    // We don't know if the user has seen any notification before as we have
    // never set which milestone was last seen. So use the version of chrome
    // where the profile was created instead.
    base::Version profile_version(
        ChromeVersionService::GetVersion(profile_->GetPrefs()));
    last_milestone = profile_version.components()[0];
  }
  return last_milestone < kLastChromeVersionWithReleaseNotes;
}

void ReleaseNotesStorage::MarkNotificationShown() {
  profile_->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, GetMilestone());
}

void ReleaseNotesStorage::StartShowingSuggestionChip() {
  profile_->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow,
      kTimesToShowSuggestionChip);
}

bool ReleaseNotesStorage::ShouldShowSuggestionChip() {
  const int times_left_to_show = profile_->GetPrefs()->GetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow);
  return times_left_to_show > 0;
}

void ReleaseNotesStorage::DecreaseTimesLeftToShowSuggestionChip() {
  const int times_left_to_show = profile_->GetPrefs()->GetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow);
  if (times_left_to_show == 0)
    return;
  profile_->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow,
      times_left_to_show - 1);
}

void ReleaseNotesStorage::StopShowingSuggestionChip() {
  profile_->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);
}

}  // namespace ash
