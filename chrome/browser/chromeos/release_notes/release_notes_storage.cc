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
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

int GetMilestone() {
  return version_info::GetVersion().components()[0];
}

constexpr int kTimesToShowSuggestionChip = 1;

}  // namespace

namespace chromeos {

void ReleaseNotesStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kReleaseNotesLastShownMilestone, 0);
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

  // TODO: remove after fixing http://crbug.com/991767.
  const base::CommandLine* current_command_line =
      base::CommandLine::ForCurrentProcess();
  const bool is_running_test =
      current_command_line->HasSwitch(::switches::kTestName) ||
      current_command_line->HasSwitch(::switches::kTestType);
  if (is_running_test) {
    DLOG(WARNING) << "Ignoring Release Notes Notification for test.";
    return false;
  }

  std::string user_email = profile_->GetProfileUserName();
  if (gaia::IsGoogleInternalAccountEmail(user_email) ||
      (ProfileHelper::Get()->GetUserByProfile(profile_)->HasGaiaAccount() &&
       !profile_->GetProfilePolicyConnector()->IsManaged())) {
    const int last_milestone = profile_->GetPrefs()->GetInteger(
        prefs::kReleaseNotesLastShownMilestone);
    if (last_milestone < GetMilestone()) {
      profile_->GetPrefs()->SetInteger(
          prefs::kReleaseNotesSuggestionChipTimesLeftToShow,
          kTimesToShowSuggestionChip);
      return true;
    }
  }
  return false;
}

void ReleaseNotesStorage::MarkNotificationShown() {
  profile_->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                   GetMilestone());
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
