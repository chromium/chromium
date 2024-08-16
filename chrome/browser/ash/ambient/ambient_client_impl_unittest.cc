// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ambient/ambient_client_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char16_t kTestProfileName16[] = u"user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

class AmbientClientImplTest : public testing::Test {
 public:
  AmbientClientImplTest() = default;
  ~AmbientClientImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(
        kTestProfileName, /*prefs=*/{}, kTestProfileName16,
        /*avatar_id=*/0,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());
    image_downloader_ = std::make_unique<ash::TestImageDownloader>();

    ambient_client_ = std::make_unique<AmbientClientImpl>();
  }

  void TearDown() override {
    ambient_client_.reset();
    user_manager_enabler_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_manager_.reset();
  }

 protected:
  AmbientClientImpl& ambient_client() { return *ambient_client_; }
  TestingProfile* profile() { return profile_; }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void AddAndLoginUser(const AccountId& account_id) {
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
    GetFakeUserManager()->SwitchActiveUser(account_id);
    MaybeMakeAccountAsPrimaryAccount(account_id);
  }

  ash::TestImageDownloader& image_downloader() { return *image_downloader_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 private:
  void MaybeMakeAccountAsPrimaryAccount(const AccountId& account_id) {
    if (!identity_test_env()->identity_manager()->HasPrimaryAccount(
            signin::ConsentLevel::kSignin)) {
      identity_test_env()->MakePrimaryAccountAvailable(
          account_id.GetUserEmail(), signin::ConsentLevel::kSignin);
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Owned by |profile_manager_|
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<ash::TestImageDownloader> image_downloader_;
  std::unique_ptr<AmbientClientImpl> ambient_client_;
};

TEST_F(AmbientClientImplTest, AllowedByPrimaryUser) {
  AddAndLoginUser(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_TRUE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}

TEST_F(AmbientClientImplTest, DisallowedByNonPrimaryUser) {
  AddAndLoginUser(
      AccountId::FromUserEmailGaiaId("user2@gmail.com", kTestGaiaId));
  AddAndLoginUser(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  EXPECT_FALSE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}

TEST_F(AmbientClientImplTest, DisallowedByEmailDomain) {
  AddAndLoginUser(
      AccountId::FromUserEmailGaiaId("user@gmailtest.com", kTestGaiaId));
  EXPECT_FALSE(ash::AmbientClient::Get()->IsAmbientModeAllowed());
}

TEST_F(AmbientClientImplTest, DownloadImage) {
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AddAndLoginUser(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  ambient_client().DownloadImage("test_url", base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(image_downloader().last_request_headers().IsEmpty());
  EXPECT_EQ(
      "Bearer access_token",
      image_downloader().last_request_headers().GetHeader("Authorization"));
}

TEST_F(AmbientClientImplTest, DownloadImageMultipleTimes) {
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AddAndLoginUser(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), kTestGaiaId));
  // make sure multiple images can download at the same time.
  ambient_client().DownloadImage("test_url_1", base::DoNothing());
  ambient_client().DownloadImage("test_url_2", base::DoNothing());
  ambient_client().DownloadImage("test_url_3", base::DoNothing());

  EXPECT_EQ(3u, ambient_client().token_fetchers_for_testing().size());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, ambient_client().token_fetchers_for_testing().size());
}
