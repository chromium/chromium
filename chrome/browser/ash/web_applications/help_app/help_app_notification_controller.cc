// Copyright 2021 The Chromium Authors
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

// This stores the latest milestone with new Discover Tab content. If the last
// milestone the user has seen the notification is before this, a new
// notification will be shown.
constexpr int kLastChromeVersionWithDiscoverTabContent = 97;
constexpr int kTimesToShowSuggestionChip = 3;

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

namespace help_app {
namespace prefs {

// Deprecated 06/2021.
// Obsolete pref that used to store the last milestone on which release notes
// notification was shown.
const char kObsoleteReleaseNotesLastShownMilestone[] =
    "last_release_notes_shown_milestone";

// Deprecated 06/2021.
// Obsolete pref that used to store the last milestone on which the Discover Tab
// notification was shown.
const char kObsoleteDiscoverTabNotificationLastShownMilestone[] =
    "discover_tab_notification_last_shown_milestone";

}  // namespace prefs
}  // namespace help_app

void HelpAppNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kHelpAppNotificationLastShownMilestone,
                                -10);
  registry->RegisterIntegerPref(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
}

void HelpAppNotificationController::RegisterObsoletePrefsForMigration(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      help_app::prefs::kObsoleteReleaseNotesLastShownMilestone, -10);
  registry->RegisterIntegerPref(
      help_app::prefs::kObsoleteDiscoverTabNotificationLastShownMilestone, -10);
}

void HelpAppNotificationController::MigrateObsoleteNotificationPrefs(
    PrefService* pref_service) {
  // If kHelpAppNotificationLastShownMilestone already has a value, migration
  // already happened, or we wrote to the new pref directly.
  if (pref_service->GetUserPrefValue(
          prefs::kHelpAppNotificationLastShownMilestone) != nullptr) {
    return;
  }

  // Choose the latest milestone when either a Release Notes or Discover tab
  // notification was shown.
  using help_app::prefs::kObsoleteDiscoverTabNotificationLastShownMilestone;
  using help_app::prefs::kObsoleteReleaseNotesLastShownMilestone;

  // If no user value is defined, these values default to -10.
  int release_notes_last_shown_milestone =
      pref_service->GetInteger(kObsoleteReleaseNotesLastShownMilestone);
  int discover_tab_notification_last_shown_milestone = pref_service->GetInteger(
      kObsoleteDiscoverTabNotificationLastShownMilestone);
  int latest_milestone =
      std::max(release_notes_last_shown_milestone,
               discover_tab_notification_last_shown_milestone);

  // Only set the new pref's value if any of the previous values were defined.
  if (latest_milestone > 0) {
    pref_service->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                             latest_milestone);
  }
}

void HelpAppNotificationController::ClearObsoleteNotificationPrefs(
    PrefService* pref_service) {
  pref_service->ClearPref(
      help_app::prefs::kObsoleteReleaseNotesLastShownMilestone);
  pref_service->ClearPref(
      help_app::prefs::kObsoleteDiscoverTabNotificationLastShownMilestone);
}

HelpAppNotificationController::HelpAppNotificationController(Profile* profile)
    : profile_(profile) {}

HelpAppNotificationController::~HelpAppNotificationController() = default;

// Checks profile type and when the last notification was shown to determine
// whether we should show the Discover tab notification to the user.
bool HelpAppNotificationController::ShouldShowDiscoverNotification() {
  bool should_show_for_current_channel =
      chrome::GetChannel() == version_info::Channel::STABLE ||
      base::FeatureList::IsEnabled(
          features::kHelpAppDiscoverTabNotificationAllChannels);

  if (!should_show_for_current_channel) {
    return false;
  }

  if (!profile_->IsChild()) {
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

  int last_shown_milestone = profile_->GetPrefs()->GetInteger(
      prefs::kHelpAppNotificationLastShownMilestone);
  if (profile_->GetPrefs()
          ->FindPreference(prefs::kHelpAppNotificationLastShownMilestone)
          ->IsDefaultValue()) {
    // We don't know if the user has seen any notification before as we have
    // never set which milestone was last seen. So use the version of chrome
    // where the profile was created instead.
    base::Version profile_version(
        ChromeVersionService::GetVersion(profile_->GetPrefs()));
    last_shown_milestone = profile_version.components()[0];
  }
  return last_shown_milestone < kLastChromeVersionWithDiscoverTabContent;
}

void HelpAppNotificationController::MaybeShowDiscoverNotification() {
  if (IsNotificationShownForCurrentMilestone(profile_))
    return;
  if (ShouldShowDiscoverNotification() && !discover_tab_notification_) {
    discover_tab_notification_ =
        std::make_unique<HelpAppDiscoverTabNotification>(profile_);
    discover_tab_notification_->Show();

    // Update milestone when notification is shown.
    profile_->GetPrefs()->SetInteger(
        prefs::kHelpAppNotificationLastShownMilestone, CurrentMilestone());
    // When this notification has been shown, start showing the Discover tab
    // suggestion chip in the launcher.
    profile_->GetPrefs()->SetInteger(
        prefs::kDiscoverTabSuggestionChipTimesLeftToShow,
        kTimesToShowSuggestionChip);
  }
}

void HelpAppNotificationController::MaybeShowReleaseNotesNotification() {
  if (IsNotificationShownForCurrentMilestone(profile_))
    return;
  if (!release_notes_notification_) {
    release_notes_notification_ =
        std::make_unique<ReleaseNotesNotification>(profile_);
  }
  // Let the ReleaseNotesNotification decide if it should show itself.
  release_notes_notification_->MaybeShowReleaseNotes();
}

}  // namespace ash
