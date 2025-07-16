// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/actor_login/actor_login_types.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace actor_login {

using testing::Return;

class ActorLoginDelegateImplTest : public ::testing::Test {
 public:
  ActorLoginDelegateImplTest() = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    delegate_ =
        ActorLoginDelegateImpl::GetOrCreateForWebContents(web_contents_);
  }

 protected:
  // Declare TaskEnvironment as the FIRST member to ensure proper lifetime.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;

  content::TestWebContentsFactory web_contents_factory_;
  // `raw_ptr` because `TestWebContentsFactory` owns it
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // `raw_ptr` because `WebContentsUserData` owns it
  raw_ptr<ActorLoginDelegateImpl> delegate_ = nullptr;
};

TEST_F(ActorLoginDelegateImplTest, GetCredentialsSuccess) {
  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get()
                  .value()
                  .empty());  // Delegate's default impl returns an empty vector
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsServiceBusy) {
  // Start the first request.
  base::test::TestFuture<CredentialsOrError> first_future;
  delegate_->GetCredentials(first_future.GetCallback());
  // Immediately try to start a second request.
  base::test::TestFuture<CredentialsOrError> second_future;
  delegate_->GetCredentials(second_future.GetCallback());
  // The second request should be rejected immediately with `kServiceBusy`.
  ASSERT_FALSE(second_future.Get().has_value());
  EXPECT_EQ(second_future.Get().error(), ActorLoginError::kServiceBusy);

  ASSERT_TRUE(first_future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, AttemptLoginSuccess) {
  tabs::MockTabInterface mock_tab;
  Credential credential;

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  // Delegate's impl returns a default `LoginStatusResult`, which is false.
  EXPECT_FALSE(future.Get().value().value());
}

TEST_F(ActorLoginDelegateImplTest, AttemptLoginServiceBusy) {
  Credential credential;

  // Start the first request (`AttemptLogin`).
  base::test::TestFuture<LoginStatusResultOrError> first_future;
  delegate_->AttemptLogin(credential, first_future.GetCallback());
  // Immediately try to start a second request of the same type.
  base::test::TestFuture<LoginStatusResultOrError> second_future;
  delegate_->AttemptLogin(credential, second_future.GetCallback());

  // Immediately try to start a `GetCredentials` request (different type).
  base::test::TestFuture<CredentialsOrError> third_future;
  delegate_->GetCredentials(third_future.GetCallback());

  // Both second and third request should be rejected as any request makes the
  // service busy.
  ASSERT_FALSE(second_future.Get().has_value());
  EXPECT_EQ(second_future.Get().error(), ActorLoginError::kServiceBusy);
  ASSERT_FALSE(third_future.Get().has_value());
  EXPECT_EQ(third_future.Get().error(), ActorLoginError::kServiceBusy);

  // This ensures that the first request completes, which will clear the pending
  // flag. First requests is successfully completed since service wasn't busy
  // at the time it was started.
  ASSERT_TRUE(first_future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, CallbacksAreResetAfterCompletion) {
  // First `GetCredentials` call.
  base::test::TestFuture<CredentialsOrError> future1;
  delegate_->GetCredentials(future1.GetCallback());
  ASSERT_TRUE(future1.Get().has_value());

  // Second `GetCredentials` call should now be possible.
  base::test::TestFuture<CredentialsOrError> future2;
  delegate_->GetCredentials(future2.GetCallback());
  ASSERT_TRUE(future2.Get().has_value());

  Credential credential;
  // First `AttemptLogin` call.
  base::test::TestFuture<LoginStatusResultOrError> future3;
  delegate_->AttemptLogin(credential, future3.GetCallback());
  ASSERT_TRUE(future3.Get().has_value());

  // Second `AttemptLogin` call should now be possible.
  base::test::TestFuture<LoginStatusResultOrError> future4;
  delegate_->AttemptLogin(credential, future4.GetCallback());
  ASSERT_TRUE(future4.Get().has_value());
}

}  // namespace actor_login
