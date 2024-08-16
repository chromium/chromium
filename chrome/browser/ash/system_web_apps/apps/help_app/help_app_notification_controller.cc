// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/version.h"
#include "chrome/browser/ash/release_notes/release_notes_notification.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int CurrentMilestone() {
  return version_info::GetVersion().components()[0];
}

// Checks if a notification was already shown in the current milestone.
bool IsNotificationShownForCurrentMilestone(Profile* profile) {
  int last_shown_milestone = profile->GetPrefs()->GetInteger(
      prefs::kHelpAppNotificationLastShownMilestone);
  if (profile->GetPrefs()
          ->FindPreference(prefs::kHelpAppNotificationLastShownMilestone)
          ->IsDefaultValue()) {
    // We don't know if the user has seen any notification before as we have
    // never set which milestone was last seen. So use the version of chrome
    // where the profile was created instead.
    base::Version profile_version(
        ChromeVersionService::GetVersion(profile->GetPrefs()));
    last_shown_milestone = profile_version.components()[0];
  }
  return last_shown_milestone == CurrentMilestone();
}

}  // namespace

namespace ash {

void HelpAppNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kHelpAppNotificationLastShownMilestone,
                                -10);
}

HelpAppNotificationController::HelpAppNotificationController(Profile* profile)
    : profile_(profile) {}

HelpAppNotificationController::~HelpAppNotificationController() = default;

void HelpAppNotificationController::MaybeShowReleaseNotesNotification() {
  if (IsNotificationShownForCurrentMilestone(profile_) &&
      !base::FeatureList::IsEnabled(
          features::kReleaseNotesNotificationAlwaysEligible)) {
    return;
  }
  if (features::IsForestFeatureEnabled()) {
    return;
  }
  ReleaseNotesStorage release_notes_storage(profile_);
  if (!release_notes_storage.ShouldNotify()) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          features::kHelpAppOpensInsteadOfReleaseNotesNotification)) {
    chrome::LaunchReleaseNotes(profile_, apps::LaunchSource::kFromOsLogin);
    release_notes_storage.MarkNotificationShown();
    return;
  }

  if (!release_notes_notification_) {
    release_notes_notification_ =
        std::make_unique<ReleaseNotesNotification>(profile_);
  }
  // Let the ReleaseNotesNotification decide if it should show itself.
  release_notes_notification_->MaybeShowReleaseNotes();
}

}  // namespace ash
