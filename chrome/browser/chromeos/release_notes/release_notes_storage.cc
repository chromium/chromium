// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"

#include "base/command_line.h"
#include "base/version.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/login/login_state/login_state.h"
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
  std::string user_email = profile->GetProfileUserName();
  return gaia::IsGoogleInternalAccountEmail(user_email) ||
         (chromeos::ProfileHelper::Get()
              ->GetUserByProfile(profile)
              ->HasGaiaAccount() &&
          !profile->GetProfilePolicyConnector()->IsManaged());
}

bool ShouldShowForCurrentChannel() {
  return chrome::GetChannel() == version_info::Channel::STABLE ||
         base::FeatureList::IsEnabled(
             chromeos::features::kReleaseNotesNotificationAllChannels);
}

}  // namespace

namespace chromeos {

void ReleaseNotesStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kReleaseNotesLastShownMilestone,
                                GetMilestone());
  registry->RegisterIntegerPref(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);
}

ReleaseNotesStorage::ReleaseNotesStorage(Profile* profile)
    : profile_(profile) {}

ReleaseNotesStorage::~ReleaseNotesStorage() = default;

bool ReleaseNotesStorage::ShouldNotify() {
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kReleaseNotesNotification))
    return false;

  if (!ShouldShowForCurrentChannel())
    return false;

  if (!IsEligibleProfile(profile_))
    return false;

  const int last_milestone =
      profile_->GetPrefs()->GetInteger(prefs::kReleaseNotesLastShownMilestone);
  if (last_milestone >= GetMilestone()) {
    return false;
  }
  return true;
}

void ReleaseNotesStorage::MarkNotificationShown() {
  profile_->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                   GetMilestone());
  // When the notification is shown we should also show the suggestion chip a
  // number of times.
  profile_->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow,
      kTimesToShowSuggestionChip);
}

bool ReleaseNotesStorage::ShouldShowSuggestionChip() {
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kReleaseNotesNotification)) {
    return false;
  }

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

}  // namespace chromeos
