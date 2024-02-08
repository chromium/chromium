// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_notification_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_discover_tab_notification.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_store.h"
#include "components/version_info/version_info.h"

namespace {
int CurrentMilestone() {
  return version_info::GetVersion().components()[0];
}
}  // namespace

namespace ash {

class HelpAppNotificationControllerTest : public BrowserWithTestWindowTest {
 public:
  HelpAppNotificationControllerTest() = default;
  ~HelpAppNotificationControllerTest() override = default;

  TestingProfile* CreateRegularProfile() {
    constexpr char kEmail[] = "user@gmail.com";
    LogIn(kEmail);
    auto* profile = CreateProfile(kEmail);
    // Set profile creation version, otherwise it defaults to 1.0.0.0.
    ChromeVersionService::SetVersion(
        profile->GetPrefs(), std::string(version_info::GetVersionNumber()));
    return profile;
  }

  TestingProfile* CreateChildProfile() {
    TestingProfile* profile = CreateRegularProfile();
    ChromeVersionService::SetVersion(
        profile->GetPrefs(), std::string(version_info::GetVersionNumber()));
    profile->SetIsSupervisedProfile();
    return profile;
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    help_app_notification_controller_ =
        std::make_unique<HelpAppNotificationController>(profile());
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(nullptr);
    notification_tester_->SetNotificationAddedClosure(base::BindRepeating(
        &HelpAppNotificationControllerTest::OnNotificationAdded,
        base::Unretained(this)));
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kHelpAppDiscoverTabNotificationAllChannels,
         features::kReleaseNotesNotificationAllChannels},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    help_app_notification_controller_.reset();
    notification_tester_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  bool HasDiscoverTabNotification() {
    return notification_tester_
        ->GetNotification(kShowHelpAppDiscoverTabNotificationId)
        .has_value();
  }

  bool HasReleaseNotesNotification() {
    return notification_tester_
        ->GetNotification("show_release_notes_notification")
        .has_value();
  }

  message_center::Notification GetDiscoverTabNotification() {
    return notification_tester_
        ->GetNotification(kShowHelpAppDiscoverTabNotificationId)
        .value();
  }

  message_center::Notification GetReleaseNotesNotification() {
    return notification_tester_
        ->GetNotification("show_release_notes_notification")
        .value();
  }

  int notification_count_ = 0;
  std::unique_ptr<HelpAppNotificationController>
      help_app_notification_controller_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests for regular profiles.
TEST_F(HelpAppNotificationControllerTest,
       DoesNotShowAnyNotificationIfNewRegularProfile) {
  Profile* profile = CreateRegularProfile();
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_EQ(false, HasReleaseNotesNotification());

  controller->MaybeShowDiscoverNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_EQ(false, HasDiscoverTabNotification());
}

TEST_F(HelpAppNotificationControllerTest,
       ShowsReleaseNotesNotificationIfShownInOlderMilestone) {
  Profile* profile = CreateRegularProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  20);
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();

  EXPECT_EQ(1, notification_count_);
  EXPECT_EQ(true, HasReleaseNotesNotification());
  EXPECT_EQ(CurrentMilestone(),
            profile->GetPrefs()->GetInteger(
                prefs::kHelpAppNotificationLastShownMilestone));
}

TEST_F(HelpAppNotificationControllerTest,
       DoesNotShowReleaseNotificationIfAlreadyShownInCurrentMilestone) {
  Profile* profile = CreateRegularProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  CurrentMilestone());
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_EQ(false, HasDiscoverTabNotification());
}

TEST_F(HelpAppNotificationControllerTest,
       DoesNotShowDiscoverNotificationIfNotChildProfile) {
  Profile* profile = CreateRegularProfile();
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  20);

  controller->MaybeShowDiscoverNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_EQ(false, HasDiscoverTabNotification());
}

// Tests for Child profile.
TEST_F(HelpAppNotificationControllerTest,
       DoesNotShowAnyNotificationIfNewChildProfile) {
  Profile* profile = CreateChildProfile();
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_EQ(false, HasReleaseNotesNotification());

  controller->MaybeShowDiscoverNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_EQ(false, HasDiscoverTabNotification());
}

// TODO(b/187774783): Remove this when discover tab is supported in all locales.
TEST_F(HelpAppNotificationControllerTest,
       DoesNotShowDiscoverNotificationIfSystemLanguageNotEnglish) {
  Profile* profile = CreateChildProfile();
  g_browser_process->SetApplicationLocale("fr");
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  20);
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowDiscoverNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_EQ(false, HasDiscoverTabNotification());
}

TEST_F(HelpAppNotificationControllerTest,
       ShowsDiscoverNotificationIfShownInPreviousMilestone) {
  Profile* profile = CreateChildProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  91);
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowDiscoverNotification();

  EXPECT_EQ(1, notification_count_);
  EXPECT_EQ(true, HasDiscoverTabNotification());
  EXPECT_EQ(CurrentMilestone(),
            profile->GetPrefs()->GetInteger(
                prefs::kHelpAppNotificationLastShownMilestone));
}

TEST_F(HelpAppNotificationControllerTest,
       DoesNotShowMoreThanOneNotificationPerMilestone) {
  Profile* profile = CreateChildProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  91);
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowDiscoverNotification();

  EXPECT_EQ(1, notification_count_);
  EXPECT_EQ(true, HasDiscoverTabNotification());

  controller->MaybeShowReleaseNotesNotification();

  EXPECT_EQ(1, notification_count_);
  EXPECT_EQ(false, HasReleaseNotesNotification());
}

// Tests for suggestion chips.
TEST_F(HelpAppNotificationControllerTest,
       UpdatesReleaseNotesChipPrefWhenReleaseNotesNotificationShown) {
  Profile* profile = CreateRegularProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  20);
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  EXPECT_EQ(0, profile->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));

  controller->MaybeShowReleaseNotesNotification();

  EXPECT_EQ(3, profile->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

TEST_F(HelpAppNotificationControllerTest,
       UpdatesDiscoverTabChipPrefWhenDiscoverTabNotificationShown) {
  Profile* profile = CreateChildProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  20);
  std::unique_ptr<HelpAppNotificationController> controller =
      std::make_unique<HelpAppNotificationController>(profile);

  EXPECT_EQ(0, profile->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));

  controller->MaybeShowDiscoverNotification();

  EXPECT_EQ(3, profile->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));
}

}  // namespace ash
