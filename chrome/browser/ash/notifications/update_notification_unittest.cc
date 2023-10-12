// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/update_notification.h"

#include "ash/constants/ash_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/notifications/update_notification_showing_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

class UpdateNotificationTest : public testing::Test,
                               public testing::WithParamInterface<bool> {
 public:
  UpdateNotificationTest() : local_state_(TestingBrowserProcess::GetGlobal()) {
    if (IsUpdateNotificationEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kFeatureManagementUpdateNotification,
           features::kUpdateNotification},
          {});
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kUpdateNotification);
    }
  }
  ~UpdateNotificationTest() override = default;

  bool IsUpdateNotificationEnabled() const { return GetParam(); }

  void SetUp() override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    auto profile_manager_unique =
        std::make_unique<FakeProfileManager>(user_data_dir_.GetPath());
    fake_profile_manager_ = profile_manager_unique.get();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::move(profile_manager_unique));

    // Creates the primary profile.
    DCHECK(!scoped_user_manager_) << "there can be only one primary profile";
    user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
    std::string email = "test@gmail.com";
    const AccountId account_id(AccountId::FromUserEmail(email));
    user_manager_->AddUser(account_id);
    user_manager_->LoginUser(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager_));
    base::FilePath user_profile_path =
        user_data_dir_.GetPath().Append(ProfileHelper::Get()->GetUserProfileDir(
            user_manager::FakeUserManager::GetFakeUsernameHash(account_id)));
    auto profile = std::make_unique<TestingProfile>(user_profile_path);
    profile_ = profile.get();
    fake_profile_manager_->RegisterTestingProfile(std::move(profile),
                                                  /*add_to_storage=*/false);

    // Mocks this notification has never been shown in the current milestone.
    UserSessionManager::GetInstance()
        ->GetUpdateNotificationShowingController(profile_)
        ->SetFakeCurrentMilestoneForTesting(119);
    profile_->GetPrefs()->SetInteger(
        prefs::kUpdateNotificationLastShownMilestone, -10);

    display_service_ = static_cast<StubNotificationDisplayService*>(
        NotificationDisplayServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_,
                base::BindRepeating(
                    &StubNotificationDisplayService::FactoryForTests)));
  }

  void TearDown() override {
    scoped_user_manager_.reset();

    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
  }

  absl::optional<message_center::Notification> GetNotification() {
    UserSessionManager::GetInstance()->MaybeShowUpdateNotification(
        ProfileManager::GetPrimaryUserProfile());
    return display_service_->GetNotification("chrome://update_notification");
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeChromeUserManager> user_manager_;
  raw_ptr<FakeProfileManager, DanglingUntriaged | ExperimentalAsh>
      fake_profile_manager_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir user_data_dir_;
  raw_ptr<Profile, DanglingUntriaged | ExperimentalAsh> profile_;
  raw_ptr<StubNotificationDisplayService, DanglingUntriaged | ExperimentalAsh>
      display_service_;
};

INSTANTIATE_TEST_SUITE_P(UpdateNotification,
                         UpdateNotificationTest,
                         testing::Bool());

TEST_P(UpdateNotificationTest, ShowNotification) {
  absl::optional<message_center::Notification> notification = GetNotification();

  // Should not show the update notification if the flag is not enabled.
  if (!IsUpdateNotificationEnabled()) {
    ASSERT_FALSE(notification);
    return;
  }

  // Show the update notification if the flag is enabled.
  ASSERT_TRUE(notification);
  EXPECT_EQ(
      u"New features include Magic Eraser on Google Photos to remove "
      u"distractions, improved video call tools, and more",
      notification->message());
}

}  // namespace ash
