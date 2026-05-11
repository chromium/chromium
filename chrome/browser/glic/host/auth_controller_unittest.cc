// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/auth_controller.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeGlicCookieSynchronizer::RunCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  int copy_cookies_called_count() const { return copy_cookies_called_count_; }
  void set_sync_result(bool result) { sync_result_ = result; }

  void WaitForSyncToComplete() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void RunCallback(base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(sync_result_);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  int copy_cookies_called_count_ = 0;
  bool sync_result_ = true;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<FakeGlicCookieSynchronizer> weak_ptr_factory_{this};
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
    synchronizer_ = mock_sync.get();
    auth_controller_->SetCookieSynchronizerForTesting(std::move(mock_sync));

    identity_test_env_->MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<AuthController> auth_controller_;
  raw_ptr<FakeGlicCookieSynchronizer> synchronizer_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AuthControllerTest, SkipCookieSyncOnOpen_Disabled) {
  feature_list_.InitAndDisableFeature(features::kGlicSkipCookieSyncOnOpen);
  base::HistogramTester histogram_tester;

  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());

  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Auth.CheckAuthBeforeLoadOutcome",
      glic::CheckAuthBeforeLoadOutcome::kSyncAttempted, 1);
}

TEST_F(AuthControllerTest, SkipCookieSyncOnOpen_Enabled) {
  feature_list_.InitAndEnableFeature(features::kGlicSkipCookieSyncOnOpen);
  base::HistogramTester histogram_tester;

  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());

  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 0);
  histogram_tester.ExpectUniqueSample(
      "Glic.Auth.CheckAuthBeforeLoadOutcome",
      glic::CheckAuthBeforeLoadOutcome::kSkipOnOpen, 1);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(AuthControllerTest,
       CookieSyncOnTokenChange_Disabled_PrimaryAccountChanged) {
  feature_list_.InitAndDisableFeature(features::kGlicCookieSyncOnTokenChange);

  // Trigger primary account change.
  signin::ClearPrimaryAccount(identity_test_env_->identity_manager());
  identity_test_env_->MakePrimaryAccountAvailable(
      "user2@gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 0);
}

TEST_F(AuthControllerTest, CookieSyncOnTokenChange_PrimaryAccountChanged) {
  feature_list_.InitAndEnableFeature(features::kGlicCookieSyncOnTokenChange);

  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 0);

  // Trigger primary account change.
  signin::ClearPrimaryAccount(identity_test_env_->identity_manager());
  identity_test_env_->MakePrimaryAccountAvailable(
      "user2@gmail.com", signin::ConsentLevel::kSignin);

  // Expect 1 or more calls because both OnPrimaryAccountChanged and
  // OnErrorStateOfRefreshTokenUpdatedForAccount may be triggered when setting
  // a new account.
  EXPECT_GT(synchronizer_->copy_cookies_called_count(), 0);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AuthControllerTest, CookieSyncOnTokenChange_SkipsSyncIfAlreadyDone) {
  feature_list_.InitAndEnableFeature(features::kGlicCookieSyncOnTokenChange);

  CoreAccountInfo account_info =
      identity_test_env_->identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  identity_test_env_->SetRefreshTokenForAccount(account_info.account_id);
  synchronizer_->WaitForSyncToComplete();
  ASSERT_EQ(synchronizer_->copy_cookies_called_count(), 1);

  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());

  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);
}

TEST_F(AuthControllerTest,
       CookieSyncOnTokenChange_TokenErrorTriggersSyncEventually) {
  feature_list_.InitAndEnableFeature(features::kGlicCookieSyncOnTokenChange);

  CoreAccountInfo account_info =
      identity_test_env_->identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // Trigger error update.
  identity_test_env_->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id, GoogleServiceAuthError::FromServiceError(""));

  ASSERT_EQ(synchronizer_->copy_cookies_called_count(), 0);

  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());
  ASSERT_EQ(future.Get(), mojom::PrepareForClientResult::kRequiresSignIn);

  // Resolve the error by updating the token.
  identity_test_env_->SetRefreshTokenForAccount(account_info.account_id);
  synchronizer_->WaitForSyncToComplete();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);

  // Subsequent load should be successful and not trigger another sync.
  base::test::TestFuture<mojom::PrepareForClientResult> future2;
  auth_controller_->CheckAuthBeforeLoad(future2.GetCallback());
  EXPECT_EQ(future2.Get(), mojom::PrepareForClientResult::kSuccess);
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);
}

TEST_F(AuthControllerTest,
       CookieSyncOnTokenChange_SetRefreshTokenForAccountTriggerSync) {
  feature_list_.InitAndEnableFeature(features::kGlicCookieSyncOnTokenChange);

  CoreAccountInfo account_info =
      identity_test_env_->identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  identity_test_env_->SetRefreshTokenForAccount(account_info.account_id);
  synchronizer_->WaitForSyncToComplete();
  ASSERT_EQ(synchronizer_->copy_cookies_called_count(), 1);
}

TEST_F(AuthControllerTest,
       CookieSyncOnTokenChange_QueuesCallbacksWhenSyncInProgress) {
  feature_list_.InitAndEnableFeature(features::kGlicCookieSyncOnTokenChange);

  // Issue one CheckAuthBeforeLoad(), and verify it is pending.
  base::test::TestFuture<mojom::PrepareForClientResult> future1;
  auth_controller_->CheckAuthBeforeLoad(future1.GetCallback());
  ASSERT_EQ(synchronizer_->copy_cookies_called_count(), 1);
  ASSERT_FALSE(future1.IsReady());

  // Issue a second CheckAuthBeforeLoad(), and ensure it also calls the cookie
  // synchronizer.
  base::test::TestFuture<mojom::PrepareForClientResult> future2;
  auth_controller_->CheckAuthBeforeLoad(future2.GetCallback());
  ASSERT_EQ(synchronizer_->copy_cookies_called_count(), 2);
  ASSERT_FALSE(future2.IsReady());

  // Both calls should eventually succeed.
  EXPECT_EQ(future1.Get(), mojom::PrepareForClientResult::kSuccess);
  EXPECT_EQ(future2.Get(), mojom::PrepareForClientResult::kSuccess);
}

}  // namespace glic
