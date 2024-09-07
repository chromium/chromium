// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/load_profile.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::kiosk::CryptohomeMountState;
using ash::kiosk::CryptohomeMountStateCallback;
using ash::kiosk::LoadProfileResult;
using ash::kiosk::LoadProfileWithCallbacks;
using ash::kiosk::PerformSigninResultCallback;
using ash::kiosk::StartSessionResultCallback;
using base::test::TestFuture;

namespace ash {

namespace {

constexpr std::string_view kKioskEmail = "kiosk@example.com";

// App type used in tests. Could be any `KioskAppType`.
constexpr KioskAppType kTestAppType = KioskAppType::kWebApp;

// Helper to post a task with less boilerplate.
void PostTask(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(task)));
}

// Helper to return a handle to a `CancellableJob`.
std::unique_ptr<CancellableJob> NewCancellableJob() {
  return std::unique_ptr<CancellableJob>{};
}

// Account ID used in tests. Could be any `AccountId`.
AccountId TestAccountId() {
  return AccountId::FromUserEmailGaiaId(/*user_email=*/std::string(kKioskEmail),
                                        /*gaia_id=*/"a fake gaia id");
}

// User context used in tests. Could be any `UserContext` .
UserContext TestUserContext() {
  return UserContext(user_manager::UserType::kWebKioskApp, TestAccountId());
}

}  // namespace

class LoadProfileTest : public testing::Test {
 protected:
  // Set task environment with mock time to call `PostDelayedTask`.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  LoadProfileTest() = default;
};

TEST_F(LoadProfileTest, ReturnsProfileOnSuccess) {
  TestingProfile profile;
  profile.set_profile_name(std::string(kKioskEmail));

  auto fake_check_cryptohome = [](CryptohomeMountStateCallback cb) {
    PostTask(base::BindOnce(std::move(cb), CryptohomeMountState::kNotMounted));
    return NewCancellableJob();
  };

  auto fake_signin = [](KioskAppType app_type, AccountId id,
                        PerformSigninResultCallback cb) {
    PostTask(base::BindOnce(std::move(cb), TestUserContext()));
    return NewCancellableJob();
  };

  auto fake_start_session = [&](const UserContext& user_context,
                                StartSessionResultCallback cb) {
    PostTask(base::BindOnce(std::move(cb), std::ref(profile)));
    return NewCancellableJob();
  };

  TestFuture<LoadProfileResult> future;
  auto handle = LoadProfileWithCallbacks(
      TestAccountId(), kTestAppType, base::BindOnce(fake_check_cryptohome),
      base::BindOnce(fake_signin),
      base::BindLambdaForTesting(fake_start_session), future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(profile.GetProfileUserName(), result.value()->GetProfileUserName());
}

TEST_F(LoadProfileTest, ForwardsResultsBetweenStepsCorrectly) {
  TestingProfile profile;
  profile.set_profile_name(std::string(kKioskEmail));

  AccountId seen_account_id;
  KioskAppType seen_app_type;
  UserContext seen_user_context;

  auto fake_check_cryptohome = [](CryptohomeMountStateCallback cb) {
    PostTask(base::BindOnce(std::move(cb), CryptohomeMountState::kNotMounted));
    return NewCancellableJob();
  };
  auto fake_signin = [&](KioskAppType app_type, AccountId id,
                         PerformSigninResultCallback cb) {
    seen_app_type = app_type;
    seen_account_id = TestAccountId();
    PostTask(base::BindOnce(std::move(cb), TestUserContext()));
    return NewCancellableJob();
  };
  auto fake_start_session = [&](const UserContext& user_context,
                                StartSessionResultCallback cb) {
    seen_user_context = user_context;
    PostTask(base::BindOnce(std::move(cb), std::ref(profile)));
    return NewCancellableJob();
  };

  TestFuture<LoadProfileResult> future;
  auto handle = LoadProfileWithCallbacks(
      TestAccountId(), kTestAppType, base::BindOnce(fake_check_cryptohome),
      base::BindLambdaForTesting(fake_signin),
      base::BindLambdaForTesting(fake_start_session), future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(seen_account_id, TestAccountId());
  EXPECT_EQ(seen_app_type, kTestAppType);
  EXPECT_EQ(seen_user_context, TestUserContext());
  EXPECT_EQ(result.value()->GetProfileUserName(), profile.GetProfileUserName());
}

TEST_F(LoadProfileTest, ReturnsCryptohomeCheckError) {
  bool did_call_jobs_after_failure = false;

  auto fake_check_cryptohome = [](CryptohomeMountStateCallback cb) {
    PostTask(base::BindOnce(std::move(cb), CryptohomeMountState::kMounted));
    return NewCancellableJob();
  };
  auto fake_signin = [&](KioskAppType app_type, AccountId id,
                         PerformSigninResultCallback cb) {
    did_call_jobs_after_failure = true;
    return NewCancellableJob();
  };
  auto fake_start_session = [&](const UserContext& user_context,
                                StartSessionResultCallback cb) {
    did_call_jobs_after_failure = true;
    return NewCancellableJob();
  };

  TestFuture<LoadProfileResult> future;
  auto handle = LoadProfileWithCallbacks(
      TestAccountId(), kTestAppType, base::BindOnce(fake_check_cryptohome),
      base::BindLambdaForTesting(fake_signin),
      base::BindLambdaForTesting(fake_start_session), future.GetCallback());

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), KioskAppLaunchError::Error::kAlreadyMounted);
  EXPECT_FALSE(did_call_jobs_after_failure);
}

TEST_F(LoadProfileTest, ReturnsSigninError) {
  bool did_call_jobs_after_failure = false;

  auto fake_check_cryptohome = [](CryptohomeMountStateCallback cb) {
    PostTask(base::BindOnce(std::move(cb), CryptohomeMountState::kNotMounted));
    return NewCancellableJob();
  };
  auto fake_signin = [](KioskAppType app_type, AccountId id,
                        PerformSigninResultCallback cb) {
    PostTask(base::BindOnce(
        std::move(cb),
        base::unexpected(AuthFailure(AuthFailure::UNRECOVERABLE_CRYPTOHOME))));
    return NewCancellableJob();
  };
  auto fake_start_session = [&](const UserContext& user_context,
                                StartSessionResultCallback cb) {
    did_call_jobs_after_failure = true;
    return NewCancellableJob();
  };

  TestFuture<LoadProfileResult> future;
  auto handle = LoadProfileWithCallbacks(
      TestAccountId(), kTestAppType, base::BindOnce(fake_check_cryptohome),
      base::BindOnce(fake_signin),
      base::BindLambdaForTesting(fake_start_session), future.GetCallback());

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), KioskAppLaunchError::Error::kUnableToMount);
  EXPECT_FALSE(did_call_jobs_after_failure);
}

TEST_F(LoadProfileTest, StopsWhenHandleIsCancelled) {
  TestingProfile profile;
  profile.set_profile_name(std::string(kKioskEmail));

  TestFuture<void> signin_future;
  bool did_call_jobs_after_cancellation = false;

  auto fake_check_cryptohome = [](CryptohomeMountStateCallback cb) {
    PostTask(base::BindOnce(std::move(cb), CryptohomeMountState::kNotMounted));
    return NewCancellableJob();
  };

  auto fake_signin = [&](KioskAppType app_type, AccountId id,
                         PerformSigninResultCallback cb) {
    signin_future.SetValue();
    PostTask(base::BindOnce(std::move(cb), TestUserContext()));
    return NewCancellableJob();
  };

  auto fake_start_session = [&](const UserContext& user_context,
                                StartSessionResultCallback cb) {
    did_call_jobs_after_cancellation = true;
    PostTask(base::BindOnce(std::move(cb), std::ref(profile)));
    return NewCancellableJob();
  };

  TestFuture<LoadProfileResult> future;
  auto handle = LoadProfileWithCallbacks(
      TestAccountId(), kTestAppType, base::BindOnce(fake_check_cryptohome),
      base::BindLambdaForTesting(fake_signin),
      base::BindLambdaForTesting(fake_start_session), future.GetCallback());

  // Cancel the job handle after signin.
  ASSERT_NE(handle, nullptr);
  ASSERT_TRUE(signin_future.Wait()) << "Did not signin";
  handle.reset();

  base::RunLoop().RunUntilIdle();  // Wait any ongoing tasks to complete.
  ASSERT_FALSE(did_call_jobs_after_cancellation);
  ASSERT_FALSE(future.IsReady());
}

}  // namespace ash
