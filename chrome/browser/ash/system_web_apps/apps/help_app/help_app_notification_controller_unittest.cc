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
#include "google_apis/gaia/gaia_id.h"

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
    const GaiaId kFakeGaia("fakegaia");
    LogIn(kEmail, kFakeGaia);
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

    // The notification is turned off by default since there is a chip shown in
    // the birch bar. Turn it on for this test.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kReleaseNotesNotificationAllChannels,
         features::kHelpAppOpensInsteadOfReleaseNotesNotification,
         features::kReleaseNotesNotificationAlwaysEligible},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    help_app_notification_controller_.reset();
    notification_tester_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void OnNotificationAdded() { notification_count_++; }

  void TurnOffNotificationEligible() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kReleaseNotesNotificationAllChannels,
         features::kHelpAppOpensInsteadOfReleaseNotesNotification},
        /*disabled_features=*/{
            features::kReleaseNotesNotificationAlwaysEligible});
  }

 protected:
  bool HasReleaseNotesNotification() {
    return notification_tester_
        ->GetNotification("show_release_notes_notification")
        .has_value();
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

class HelpAppNotificationControllerTestWithHelpAppOpensInsteadDisabled
    : public HelpAppNotificationControllerTest {
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kReleaseNotesNotificationAllChannels,
        },
        /*disabled_features=*/{
            features::kReleaseNotesNotificationAlwaysEligible,
            features::kHelpAppOpensInsteadOfReleaseNotesNotification});
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
  }
};

// Tests for regular profiles.
TEST_F(HelpAppNotificationControllerTestWithHelpAppOpensInsteadDisabled,
       DoesNotShowAnyNotificationIfNewRegularProfile) {
  Profile* profile = CreateRegularProfile();
  auto controller = std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();
  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasReleaseNotesNotification());
}

TEST_F(HelpAppNotificationControllerTestWithHelpAppOpensInsteadDisabled,
       DoesNotShowsReleaseNotesNotificationIfShownInOlderMilestone) {
  Profile* profile = CreateRegularProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  20);
  auto controller = std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();
  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasReleaseNotesNotification());
}

TEST_F(HelpAppNotificationControllerTestWithHelpAppOpensInsteadDisabled,
       DoesNotShowReleaseNotificationIfAlreadyShownInCurrentMilestone) {
  Profile* profile = CreateRegularProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  CurrentMilestone());
  auto controller = std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();
  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasReleaseNotesNotification());
}

// Tests for Child profile.
TEST_F(HelpAppNotificationControllerTestWithHelpAppOpensInsteadDisabled,
       DoesNotShowAnyNotificationIfNewChildProfile) {
  Profile* profile = CreateChildProfile();
  auto controller = std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();
  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasReleaseNotesNotification());
}

// Tests that help app opens instead of release notes notification by default.
TEST_F(HelpAppNotificationControllerTest, DoesNotShowNotification) {
  Profile* profile = CreateRegularProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  91);
  auto controller = std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasReleaseNotesNotification());
  EXPECT_EQ(CurrentMilestone(),
            profile->GetPrefs()->GetInteger(
                prefs::kHelpAppNotificationLastShownMilestone));
}

// Tests that release notes don't auto open if the notification eligible feature
// is disabled.
TEST_F(HelpAppNotificationControllerTest,
       DoesNotOpenHelpAppIfBirchFeatureEnabled) {
  TurnOffNotificationEligible();
  Profile* profile = CreateRegularProfile();
  profile->GetPrefs()->SetInteger(prefs::kHelpAppNotificationLastShownMilestone,
                                  91);
  auto controller = std::make_unique<HelpAppNotificationController>(profile);

  controller->MaybeShowReleaseNotesNotification();
  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasReleaseNotesNotification());
  EXPECT_EQ(91, profile->GetPrefs()->GetInteger(
                    prefs::kHelpAppNotificationLastShownMilestone));
}

}  // namespace ash
