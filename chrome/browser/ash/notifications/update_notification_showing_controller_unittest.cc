// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/update_notification_showing_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/user_manager/scoped_user_manager.h"

namespace ash {

class UpdateNotificationShowingControllerTest
    : public BrowserWithTestWindowTest {
 public:
  UpdateNotificationShowingControllerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kUpdateNotification,
         features::kFeatureManagementUpdateNotification},
        /*disabled_features=*/{});
  }
  ~UpdateNotificationShowingControllerTest() override = default;

  void SetUpProfile() {
    TestingProfile::Builder builder;
    DCHECK(!scoped_user_manager_) << "there can be only one primary profile";
    user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
    if (is_guest_) {
      builder.SetGuestSession();
    } else {
      AccountId account_id_ = AccountId::FromUserEmailGaiaId(email_, "12345");
      user_manager_->AddUser(account_id_);
      builder.SetProfileName(email_);

      builder.OverridePolicyConnectorIsManagedForTesting(is_managed_);
      if (is_ephemeral_) {
        // Enabling ephemeral users passes the `IsEphemeralUserProfile` check.
        user_manager_->set_ephemeral_mode_config(
            user_manager::UserManager::EphemeralModeConfig(
                /*included_by_default=*/true,
                /*include_list=*/std::vector<AccountId>{},
                /*exclude_list=*/std::vector<AccountId>{}));
      } else if (is_unicorn_) {
        user_manager_->set_current_user_child(true);
        builder.SetIsSupervisedProfile();
      }
    }
    profile_ = builder.Build();

    notification_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
    notification_tester_->SetNotificationAddedClosure(base::BindRepeating(
        &UpdateNotificationShowingControllerTest::OnNotificationAdded,
        base::Unretained(this)));

    update_notification_controller_ =
        std::make_unique<UpdateNotificationShowingController>(profile_.get());
    if (current_milestone_ > 0) {
      update_notification_controller_->SetFakeCurrentMilestoneForTesting(
          current_milestone_);
    }
    if (last_seen_milestone_ > 0) {
      profile_->GetPrefs()->SetInteger(
          prefs::kUpdateNotificationLastShownMilestone, last_seen_milestone_);
    }
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager_));
  }

  void TearDown() override {
    user_manager_.reset();
    scoped_user_manager_.reset();
    update_notification_controller_.reset();
    notification_tester_.reset();
    pref_service_.reset();
    profile_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  bool HasUpdateNotification() {
    return notification_tester_->GetNotification("chrome://update_notification")
        .has_value();
  }

  int CurrentMilestone() {
    return update_notification_controller_->current_milestone_;
  }

  std::unique_ptr<FakeChromeUserManager> user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  int notification_count_ = 0;
  std::unique_ptr<UpdateNotificationShowingController>
      update_notification_controller_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  raw_ptr<StubNotificationDisplayService, ExperimentalAsh> display_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<Profile> profile_;

  // Data members for `SetUpProfile()`.
  std::string email_ = "test@gmail.com";
  bool is_guest_ = false;
  bool is_managed_ = false;
  bool is_ephemeral_ = false;
  bool is_unicorn_ = false;
  int current_milestone_ = -20;
  int last_seen_milestone_ = -30;
};

TEST_F(UpdateNotificationShowingControllerTest,
       ShowsUpdateNotificationIfNeverBeenShown) {
  current_milestone_ = 119;
  last_seen_milestone_ = -10;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(1, notification_count_);
  EXPECT_TRUE(HasUpdateNotification());
  EXPECT_EQ(CurrentMilestone(),
            profile_->GetPrefs()->GetInteger(
                prefs::kUpdateNotificationLastShownMilestone));
}

TEST_F(UpdateNotificationShowingControllerTest,
       NotShowsNotificationIfShownOnce) {
  current_milestone_ = 119;
  last_seen_milestone_ = -10;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(1, notification_count_);
  EXPECT_TRUE(HasUpdateNotification());

  update_notification_controller_->MaybeShowUpdateNotification();
  EXPECT_EQ(1, notification_count_);

  EXPECT_EQ(CurrentMilestone(),
            profile_->GetPrefs()->GetInteger(
                prefs::kUpdateNotificationLastShownMilestone));
}

TEST_F(UpdateNotificationShowingControllerTest,
       NotShowUpdateNotificationIfAlreadyShown) {
  current_milestone_ = 119;
  last_seen_milestone_ = 118;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasUpdateNotification());
}

TEST_F(UpdateNotificationShowingControllerTest,
       NotShowNotificationIfAlreadyShownInCurrentMilestone) {
  current_milestone_ = 119;
  last_seen_milestone_ = 119;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasUpdateNotification());
}

TEST_F(UpdateNotificationShowingControllerTest,
       NotShowNotificationIfEphemeralUser) {
  current_milestone_ = 119;
  is_ephemeral_ = true;
  last_seen_milestone_ = -10;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasUpdateNotification());
}

TEST_F(UpdateNotificationShowingControllerTest,
       NotShowNotificationIfGuestUser) {
  current_milestone_ = 119;
  is_guest_ = true;
  last_seen_milestone_ = -10;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasUpdateNotification());
}

TEST_F(UpdateNotificationShowingControllerTest,
       NotShowNotificationIfNonGooglerManagedProfile) {
  current_milestone_ = 119;
  is_managed_ = true;
  last_seen_milestone_ = -10;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(0, notification_count_);
  EXPECT_FALSE(HasUpdateNotification());
}

TEST_F(UpdateNotificationShowingControllerTest,
       ShouldShowNotificationIfGooglerManagedProfile) {
  current_milestone_ = 119;
  is_managed_ = true;
  email_ = "test@google.com";
  last_seen_milestone_ = -10;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(1, notification_count_);
  EXPECT_TRUE(HasUpdateNotification());
}

TEST_F(UpdateNotificationShowingControllerTest,
       ShouldShowNotificationIfUnicornManagedProfile) {
  current_milestone_ = 119;
  is_managed_ = true;
  is_unicorn_ = true;
  last_seen_milestone_ = -10;

  SetUpProfile();

  update_notification_controller_->MaybeShowUpdateNotification();

  EXPECT_EQ(1, notification_count_);
  EXPECT_TRUE(HasUpdateNotification());
}

}  // namespace ash
