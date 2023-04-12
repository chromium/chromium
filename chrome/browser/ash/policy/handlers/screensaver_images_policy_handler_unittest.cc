// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_images_policy_handler.h"

#include <memory>

#include "ash/public/cpp/ambient/ambient_managed_photo_source.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/test/ash_test_helper.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/repeating_test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kTestProfileDirectoryName[] = "test_profile";
constexpr char kUserEmail[] = "user@mail.com";
}  // namespace

class ScreensaverImagesPolicyHandlerTest : public testing::Test {
 public:
  ScreensaverImagesPolicyHandlerTest() = default;

  ScreensaverImagesPolicyHandlerTest(
      const ScreensaverImagesPolicyHandlerTest&) = delete;
  ScreensaverImagesPolicyHandlerTest& operator=(
      const ScreensaverImagesPolicyHandlerTest&) = delete;

  ~ScreensaverImagesPolicyHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ash_test_helper_.SetUp(ash::AshTestHelper::InitParams());

    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    fake_profile_dir_ =
        temp_dir_.GetPath().AppendASCII(kTestProfileDirectoryName);

    profile_ = TestingProfile::Builder()
                   .SetProfileName(kUserEmail)
                   .SetSharedURLLoaderFactory(
                       base::MakeRefCounted<
                           network::WeakWrapperSharedURLLoaderFactory>(
                           &url_loader_factory_))
                   .SetPath(fake_profile_dir_)
                   .Build();
  }

  void TearDown() override { policy_handler_.reset(); }

  void TriggerOnScreensaverImagesDownloaded() {
    ASSERT_TRUE(ScreensaverImagesPolicyHandler::Get());
    policy_handler_->OnScreensaverImagesDownloaded();
  }

  void RegisterUser(const AccountId& account_id,
                    std::unique_ptr<PrefService> pref_service) {
    ASSERT_TRUE(profile_);
    const user_manager::User* user =
        fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, false, user_manager::USER_TYPE_REGULAR, profile_.get());
    fake_user_manager_->UserLoggedIn(
        user->GetAccountId(), user->username_hash(), true /* browser_restart */,
        false /* is_child */);
    fake_user_manager_->SwitchActiveUser(account_id);

    ash::TestSessionControllerClient* session_controller_client =
        ash_test_helper_.test_session_controller_client();
    session_controller_client->Reset();
    session_controller_client->AddUserSession(kUserEmail,
                                              user_manager::USER_TYPE_REGULAR,
                                              /*provide_pref_service =*/false);
    session_controller_client->SetUserPrefService(account_id,
                                                  std::move(pref_service));
    session_controller_client->SwitchActiveUser(account_id);
    session_controller_client->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  void CreateHandlerInstanceWithUserProfile() {
    policy_handler_ = std::make_unique<ScreensaverImagesPolicyHandler>();

    // Verify that the handler is instantiated without an image donwloader.
    EXPECT_FALSE(policy_handler_->image_downloader_);

    // Create a fake user prefs map.
    auto user_prefs = std::make_unique<TestingPrefServiceSimple>();
    ash::RegisterUserProfilePrefs(user_prefs->registry(), /*for_test=*/true);
    ScreensaverImagesPolicyHandler::RegisterPrefs(user_prefs->registry());

    // Keep a raw pointer to the user prefs before transferring ownership.
    user_prefs_ = user_prefs.get();
    RegisterUser(AccountId::FromUserEmail(kUserEmail), std::move(user_prefs));

    // Verify that the policy handler detected the new user and created a new
    // image downloader instance.
    EXPECT_TRUE(policy_handler_->image_downloader_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  ash::AshTestHelper ash_test_helper_;
  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  base::ScopedTempDir temp_dir_;
  base::FilePath fake_profile_dir_;
  std::unique_ptr<TestingProfile> profile_;

  // Ownership of this pref service is transferred to the session controller
  TestingPrefServiceSimple* user_prefs_ = nullptr;

  network::TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<ScreensaverImagesPolicyHandler> policy_handler_;
};

TEST_F(ScreensaverImagesPolicyHandlerTest, SingletonInitialization) {
  EXPECT_EQ(nullptr, ScreensaverImagesPolicyHandler::Get());

  {
    std::unique_ptr<ScreensaverImagesPolicyHandler> handler_instance =
        std::make_unique<ScreensaverImagesPolicyHandler>();

    EXPECT_EQ(handler_instance.get(), ScreensaverImagesPolicyHandler::Get());
  }

  EXPECT_EQ(nullptr, ScreensaverImagesPolicyHandler::Get());
}

TEST_F(ScreensaverImagesPolicyHandlerTest, ShouldRunCallbackIfImagesUpdated) {
  CreateHandlerInstanceWithUserProfile();
  base::test::RepeatingTestFuture<std::vector<base::FilePath>> test_future;
  ScreensaverImagesPolicyHandler::Get()->SetScreensaverImagesUpdatedCallback(
      test_future.GetCallback<const std::vector<base::FilePath>&>());

  // Expect callbacks when images are downloaded.
  TriggerOnScreensaverImagesDownloaded();
  EXPECT_TRUE(test_future.Wait());
  test_future.Take();
  TriggerOnScreensaverImagesDownloaded();
  EXPECT_TRUE(test_future.Wait());
  test_future.Take();
  EXPECT_TRUE(test_future.IsEmpty());
}

}  // namespace policy
