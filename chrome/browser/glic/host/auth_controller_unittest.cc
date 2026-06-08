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

  void WaitForSyncToComplete() { std::ignore = sync_complete_future_.Take(); }

 private:
  void RunCallback(base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(sync_result_);
    sync_complete_future_.SetValue(true);
  }

  int copy_cookies_called_count_ = 0;
  bool sync_result_ = true;
  base::test::TestFuture<bool> sync_complete_future_{
      base::test::TestFutureMode::kQueue};
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
    default_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            features::kGlicCookieSyncOnTokenChange,
            features::kGlicCookieSyncOnOpenEvenIfNoSyncNeeded,
            features::kGlicCookieSyncOnError});
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
  base::test::ScopedFeatureList default_feature_list_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AuthControllerTest, SkipCookieSyncOnOpen_Disabled) {
  feature_list_.InitAndDisableFeature(features::kGlicSkipCookieSyncOnOpen);
  base::HistogramTester histogram_tester;

  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());

  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
  synchronizer_->WaitForSyncToComplete();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Auth.CheckAuthBeforeLoadOutcome",
      glic::CheckAuthBeforeLoadOutcome::kSyncAttempted, 1);

  histogram_tester.ExpectUniqueSample(
      "Glic.CookieSynchronization.SuccessByTrigger",
      GlicCookieSyncTrigger::kCheckAuthBeforeLoad, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.CookieSynchronization.FailureByTrigger", 0);
}

TEST_F(AuthControllerTest, CookieSyncFailureHistograms) {
  base::HistogramTester histogram_tester;
  synchronizer_->set_sync_result(false);

  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());

  EXPECT_EQ(future.Get(),
            mojom::PrepareForClientResult::kErrorResyncingCookies);
  synchronizer_->WaitForSyncToComplete();
  histogram_tester.ExpectUniqueSample(
      "Glic.CookieSynchronization.FailureByTrigger",
      GlicCookieSyncTrigger::kCheckAuthBeforeLoad, 1);
  histogram_tester.ExpectTotalCount(
      "Glic.CookieSynchronization.SuccessByTrigger", 0);
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
  base::HistogramTester histogram_tester;

  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 0);

  // Trigger primary account change.
  signin::ClearPrimaryAccount(identity_test_env_->identity_manager());
  identity_test_env_->MakePrimaryAccountAvailable(
      "user2@gmail.com", signin::ConsentLevel::kSignin);

  synchronizer_->WaitForSyncToComplete();

  EXPECT_GT(synchronizer_->copy_cookies_called_count(), 0);
  histogram_tester.ExpectBucketCount(
      "Glic.CookieSynchronization.SuccessByTrigger",
      GlicCookieSyncTrigger::kOnPrimaryAccountChanged, 1);
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
  base::HistogramTester histogram_tester;

  CoreAccountInfo account_info =
      identity_test_env_->identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  identity_test_env_->SetRefreshTokenForAccount(account_info.account_id);
  synchronizer_->WaitForSyncToComplete();
  ASSERT_EQ(synchronizer_->copy_cookies_called_count(), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.CookieSynchronization.SuccessByTrigger",
      GlicCookieSyncTrigger::kOnRefreshTokenUpdated, 1);
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

  // Both syncs should have completed.
  synchronizer_->WaitForSyncToComplete();
  synchronizer_->WaitForSyncToComplete();
}

TEST_F(AuthControllerTest, CookieSyncOnError_Disabled) {
  // If kGlicCookieSyncOnError is disabled (but kGlicCookieSyncOnTokenChange is
  // enabled), OnClientError should NOT trigger a sync immediately.
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kGlicCookieSyncOnTokenChange},
      /*disabled_features=*/{features::kGlicCookieSyncOnError});

  auth_controller_->OnClientError();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 0);

  // However, calling CheckAuthBeforeLoad should attempt a sync.
  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);
  synchronizer_->WaitForSyncToComplete();
  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
}

TEST_F(AuthControllerTest, CookieSyncOnError_Enabled) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kGlicCookieSyncOnError, {{"min_interval", "5m"}}},
       {features::kGlicCookieSyncOnTokenChange, {}}},
      {});

  // OnClientError should trigger sync immediately.
  auth_controller_->OnClientError();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);

  synchronizer_->WaitForSyncToComplete();

  // Subsequent CheckAuthBeforeLoad should NOT attempt sync because sync is
  // already complete.
  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);
  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
}

TEST_F(AuthControllerTest, CookieSyncOnError_Tolerance) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kGlicCookieSyncOnError, {{"min_interval", "5m"}}},
       {features::kGlicCookieSyncOnTokenChange, {}}},
      {});

  // First error should trigger sync immediately.
  auth_controller_->OnClientError();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);
  synchronizer_->WaitForSyncToComplete();

  // Second error immediately after should NOT trigger immediate sync.
  auth_controller_->OnClientError();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);

  // Fast forward 4 minutes (less than the 5 minute minimum interval).
  task_environment_.FastForwardBy(base::Minutes(4));
  auth_controller_->OnClientError();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);

  // But calling CheckAuthBeforeLoad should still trigger sync because sync is
  // needed.
  {
    base::test::TestFuture<mojom::PrepareForClientResult> future;
    auth_controller_->CheckAuthBeforeLoad(future.GetCallback());
    EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 2);
    synchronizer_->WaitForSyncToComplete();
    EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
  }

  // Clear interval by fast forwarding 6 minutes.
  task_environment_.FastForwardBy(base::Minutes(6));

  // Trigger error again.
  auth_controller_->OnClientError();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 3);
  synchronizer_->WaitForSyncToComplete();

  // Fast forward another 5 minutes and 1 second (exceeds the 5 minute
  // interval).
  task_environment_.FastForwardBy(base::Seconds(301));
  auth_controller_->OnClientError();
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 4);
  synchronizer_->WaitForSyncToComplete();
}

TEST_F(AuthControllerTest, CookieSyncOnError_TransientError) {
  feature_list_.InitWithFeaturesAndParameters(
      {{features::kGlicCookieSyncOnError, {{"min_interval", "5m"}}},
       {features::kGlicCookieSyncOnTokenChange, {}}},
      {});

  // kUnauthenticated transient error should trigger sync immediately.
  auth_controller_->OnClientTransientError(
      mojo_base::mojom::AbslStatusCode::kUnauthenticated);
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 1);
  synchronizer_->WaitForSyncToComplete();

  // Fast forward 6 minutes to clear the interval.
  task_environment_.FastForwardBy(base::Minutes(6));

  // kInternal transient error should trigger sync.
  auth_controller_->OnClientTransientError(
      mojo_base::mojom::AbslStatusCode::kInternal);
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 2);
  synchronizer_->WaitForSyncToComplete();

  // Fast forward 6 minutes to clear the interval.
  task_environment_.FastForwardBy(base::Minutes(6));

  // kNotFound transient error should NOT trigger sync.
  auth_controller_->OnClientTransientError(
      mojo_base::mojom::AbslStatusCode::kNotFound);
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 2);

  // And CheckAuthBeforeLoad should not trigger sync after kNotFound since
  // kNotFound does not need sync.
  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 2);
  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);
}

TEST_F(AuthControllerTest, CookieSyncOnOpenEvenIfNoSyncNeeded_Enabled) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kGlicCookieSyncOnTokenChange,
                            features::kGlicCookieSyncOnOpenEvenIfNoSyncNeeded},
      /*disabled_features=*/{});

  // Prepare a successful sync beforehand to mark needs_sync as false.
  CoreAccountInfo account_info =
      identity_test_env_->identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  identity_test_env_->SetRefreshTokenForAccount(account_info.account_id);
  synchronizer_->WaitForSyncToComplete();
  ASSERT_EQ(synchronizer_->copy_cookies_called_count(), 1);

  // Call CheckAuthBeforeLoad. Since kGlicCookieSyncOnOpenEvenIfNoSyncNeeded is
  // enabled, it should trigger another sync in the background immediately.
  base::test::TestFuture<mojom::PrepareForClientResult> future;
  auth_controller_->CheckAuthBeforeLoad(future.GetCallback());

  // We should not wait for the background sync to finish before the load
  // callback returns.
  EXPECT_EQ(future.Get(), mojom::PrepareForClientResult::kSuccess);

  // A new sync should have been triggered in the background.
  EXPECT_EQ(synchronizer_->copy_cookies_called_count(), 2);

  // Wait for the background sync to complete to cleanup.
  synchronizer_->WaitForSyncToComplete();
}

}  // namespace glic
