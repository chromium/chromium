// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/help_app/help_app_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/version.h"
#include "chrome/browser/ash/release_notes/release_notes_notification.h"
#include "chrome/browser/ash/web_applications/help_app/help_app_discover_tab_notification.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

int CurrentMilestone() {
  return version_info::GetVersion().components()[0];
}

// Checks profile type and when the last notification was shown to determine
// whether we should show the Discover tab notification to the user.
bool ShouldShowDiscoverTabNotification(Profile* profile) {
  if (!base::FeatureList::IsEnabled(chromeos::features::kHelpAppDiscoverTab)) {
    return false;
  }

  bool shouldShowForCurrentChannel =
      chrome::GetChannel() == version_info::Channel::STABLE ||
      base::FeatureList::IsEnabled(
          ash::features::kHelpAppDiscoverTabNotificationAllChannels);

  if (!shouldShowForCurrentChannel) {
    return false;
  }

  if (!profile->IsChild()) {
    return false;
  }

  // We only support the discover tab in English at the moment.
  // TODO(b/187774783): Remove this when discover tab is supported in all
  // locales.
  const std::string global_app_locale =
      g_browser_process->GetApplicationLocale();
  std::string language_code = l10n_util::GetLanguage(global_app_locale);
  if (language_code != "en") {
    return false;
  }

  int last_shown_milestone = profile->GetPrefs()->GetInteger(
      prefs::kDiscoverTabNotificationLastShownMilestone);
  if (profile->GetPrefs()
          ->FindPreference(prefs::kDiscoverTabNotificationLastShownMilestone)
          ->IsDefaultValue()) {
    // We don't know if the user has seen any notification before as we have
    // never set which milestone was last seen. So use the version of chrome
    // where the profile was created instead.
    base::Version profile_version(
        ChromeVersionService::GetVersion(profile->GetPrefs()));
    last_shown_milestone = profile_version.components()[0];
  }
  return last_shown_milestone < CurrentMilestone();
}

}  // namespace

namespace chromeos {

void HelpAppNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDiscoverTabNotificationLastShownMilestone, -10);
  registry->RegisterIntegerPref(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
}

HelpAppNotificationController::HelpAppNotificationController(Profile* profile)
    : profile_(profile) {}

HelpAppNotificationController::~HelpAppNotificationController() = default;

void HelpAppNotificationController::MaybeShowDiscoverNotification() {
  if (ShouldShowDiscoverTabNotification(profile_) &&
      !discover_tab_notification_) {
    discover_tab_notification_ =
        std::make_unique<HelpAppDiscoverTabNotification>(profile_);
    discover_tab_notification_->Show();

    // Update milestone when notification is shown.
    profile_->GetPrefs()->SetInteger(
        prefs::kDiscoverTabNotificationLastShownMilestone, CurrentMilestone());
    // When this notification has been shown, start showing the Discover tab
    // suggestion chip in the launcher.
    profile_->GetPrefs()->SetInteger(
        prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);
  }
}

void HelpAppNotificationController::MaybeShowNotification() {
  if (!release_notes_notification_) {
    release_notes_notification_ =
        std::make_unique<ash::ReleaseNotesNotification>(profile_);
    // Let the ReleaseNotesNotification decide if it should show itself.
    release_notes_notification_->MaybeShowReleaseNotes();
  }
}

}  // namespace chromeos
