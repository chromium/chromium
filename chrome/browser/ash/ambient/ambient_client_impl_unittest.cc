// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ambient/ambient_client_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr GaiaId::Literal kTestGaiaId("1234567890");

class AmbientClientImplTest : public testing::Test {
 public:
  AmbientClientImplTest() = default;
  ~AmbientClientImplTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_user_manager_controller_ =
        std::make_unique<ash::ProfileUserManagerController>(
            profile_manager_->profile_manager(), user_manager_.Get());

    image_downloader_ = std::make_unique<ash::TestImageDownloader>();
    ambient_client_ = std::make_unique<AmbientClientImpl>();
  }

  void TearDown() override {
    ambient_client_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    profile_manager_.reset();

    profile_user_manager_controller_.reset();
  }

 protected:
  AmbientClientImpl& ambient_client() { return *ambient_client_; }
  TestingProfile* profile() { return profile_; }

  void AddAndLoginUser(const AccountId& account_id) {
    user_manager_->EnsureUser(account_id, user_manager::UserType::kRegular,
                              /*is_ephemeral=*/false);
    user_manager_->UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));

    profile_ = profile_manager_->CreateTestingProfile(
        account_id.GetUserEmail(), /*prefs=*/{},
        base::UTF8ToUTF16(account_id.GetUserEmail()),
        /*avatar_id=*/0,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    if (!identity_test_env()->identity_manager()->HasPrimaryAccount(
            signin::ConsentLevel::kSignin)) {
      identity_test_env()->MakePrimaryAccountAvailable(
          account_id.GetUserEmail(), signin::ConsentLevel::kSignin);
    }
  }

  ash::TestImageDownloader& image_downloader() { return *image_downloader_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  ash::ScopedStubInstallAttributes install_attributes_;
  ash::ScopedTestingCrosSettings testing_cros_settings_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<user_manager::FakeUserManagerDelegate>(),
          TestingBrowserProcess::GetGlobal()->local_state(),
          ash::CrosSettings::Get())};
  std::unique_ptr<ash::ProfileUserManagerController>
      profile_user_manager_controller_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Owned by |profile_manager_|
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<ash::TestImageDownloader> image_downloader_;
  std::unique_ptr<AmbientClientImpl> ambient_client_;
};

TEST_F(AmbientClientImplTest, AllowedByPrimaryUser) {
  AddAndLoginUser(
      AccountId::FromUserEmailGaiaId(kTestProfileName, kTestGaiaId));
  EXPECT_TRUE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}

TEST_F(AmbientClientImplTest, DisallowedByNonPrimaryUser) {
  auto& user_manager = CHECK_DEREF(user_manager::UserManager::Get());

  // Add primary logged in user first.
  {
    const auto account_id =
        AccountId::FromUserEmailGaiaId("user2@gmail.com", GaiaId("987654321"));
    user_manager.EnsureUser(account_id, user_manager::UserType::kRegular,
                            /*is_ephemeral=*/false);
    user_manager.UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));
  }

  auto account_id =
      AccountId::FromUserEmailGaiaId(kTestProfileName, kTestGaiaId);
  AddAndLoginUser(account_id);
  // On secondary log-in, active user switch happens asynchronously.
  // Invoke the method here explicitly to simulate it.
  user_manager.SwitchActiveUser(account_id);
  EXPECT_FALSE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}

TEST_F(AmbientClientImplTest, DisallowedByEmailDomain) {
  AddAndLoginUser(
      AccountId::FromUserEmailGaiaId("user@gmailtest.com", kTestGaiaId));
  EXPECT_FALSE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}

TEST_F(AmbientClientImplTest, DownloadImage) {
  AddAndLoginUser(
      AccountId::FromUserEmailGaiaId(kTestProfileName, kTestGaiaId));
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  ambient_client().DownloadImage("test_url", base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(image_downloader().last_request_headers().IsEmpty());
  EXPECT_EQ(
      "Bearer access_token",
      image_downloader().last_request_headers().GetHeader("Authorization"));
}

TEST_F(AmbientClientImplTest, DownloadImageMultipleTimes) {
  AddAndLoginUser(
      AccountId::FromUserEmailGaiaId(kTestProfileName, kTestGaiaId));
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  // make sure multiple images can download at the same time.
  ambient_client().DownloadImage("test_url_1", base::DoNothing());
  ambient_client().DownloadImage("test_url_2", base::DoNothing());
  ambient_client().DownloadImage("test_url_3", base::DoNothing());

  EXPECT_EQ(3u, ambient_client().token_fetchers_for_testing().size());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, ambient_client().token_fetchers_for_testing().size());
}
