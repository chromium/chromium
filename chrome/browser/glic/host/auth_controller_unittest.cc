// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/auth_controller.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class FakeGlicCookieSynchronizer : public GlicCookieSynchronizer {
 public:
  FakeGlicCookieSynchronizer(content::BrowserContext* context,
                             signin::IdentityManager* identity_manager)
      : GlicCookieSynchronizer(context, identity_manager) {}

  void CopyCookiesToWebviewStoragePartition(
      base::OnceCallback<void(bool)> callback) override {
    copy_cookies_called_count_++;
    std::move(callback).Run(sync_result_);
  }

  int copy_cookies_called_count() const { return copy_cookies_called_count_; }
  void set_sync_result(bool result) { sync_result_ = result; }

 private:
  int copy_cookies_called_count_ = 0;
  bool sync_result_ = true;
};

}  // namespace

class AuthControllerTest : public testing::Test {
 public:
  AuthControllerTest() = default;
  ~AuthControllerTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();

    auth_controller_ = std::make_unique<AuthController>(
        profile_.get(), identity_test_env_->identity_manager());

    auto mock_sync = std::make_unique<FakeGlicCookieSynchronizer>(
        profile_.get(), identity_test_env_->identity_manager());
    mock_synchronizer_ = mock_sync.get();
    auth_controller_->SetCookieSynchronizerForTesting(std::move(mock_sync));

    identity_test_env_->MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<AuthController> auth_controller_;
  raw_ptr<FakeGlicCookieSynchronizer> mock_synchronizer_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AuthControllerTest, SkipCookieSyncOnOpen_Disabled) {
  feature_list_.InitAndDisableFeature(features::kGlicSkipCookieSyncOnOpen);

  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());

  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
  EXPECT_EQ(mock_synchronizer_->copy_cookies_called_count(), 1);
}

TEST_F(AuthControllerTest, SkipCookieSyncOnOpen_Enabled) {
  feature_list_.InitAndEnableFeature(features::kGlicSkipCookieSyncOnOpen);

  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());

  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
  EXPECT_EQ(mock_synchronizer_->copy_cookies_called_count(), 0);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(AuthControllerTest,
       CookieSyncOnTokenChange_Disabled_PrimaryAccountChanged) {
  feature_list_.InitAndDisableFeature(features::kGlicCookieSyncOnTokenChange);

  // Trigger primary account change.
  signin::ClearPrimaryAccount(identity_test_env_->identity_manager());
  identity_test_env_->MakePrimaryAccountAvailable(
      "user2@gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_EQ(mock_synchronizer_->copy_cookies_called_count(), 0);
}

TEST_F(AuthControllerTest,
       CookieSyncOnTokenChange_Enabled_PrimaryAccountChanged) {
  feature_list_.InitAndEnableFeature(features::kGlicCookieSyncOnTokenChange);

  EXPECT_EQ(mock_synchronizer_->copy_cookies_called_count(), 0);

  // Trigger primary account change.
  signin::ClearPrimaryAccount(identity_test_env_->identity_manager());
  identity_test_env_->MakePrimaryAccountAvailable(
      "user2@gmail.com", signin::ConsentLevel::kSignin);

  // Expect 1 or more calls because both OnPrimaryAccountChanged and
  // OnErrorStateOfRefreshTokenUpdatedForAccount may be triggered when setting
  // a new account.
  EXPECT_GT(mock_synchronizer_->copy_cookies_called_count(), 0);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AuthControllerTest, CookieSyncOnTokenChange_Disabled_TokenError) {
  feature_list_.InitAndDisableFeature(features::kGlicCookieSyncOnTokenChange);

  CoreAccountInfo account_info =
      identity_test_env_->identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // Trigger error update.
  identity_test_env_->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(mock_synchronizer_->copy_cookies_called_count(), 0);
}

TEST_F(AuthControllerTest, CookieSyncOnTokenChange_Enabled_TokenError) {
  feature_list_.InitAndEnableFeature(features::kGlicCookieSyncOnTokenChange);

  CoreAccountInfo account_info =
      identity_test_env_->identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // Trigger error update.
  identity_test_env_->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(mock_synchronizer_->copy_cookies_called_count(), 1);
}

}  // namespace glic
