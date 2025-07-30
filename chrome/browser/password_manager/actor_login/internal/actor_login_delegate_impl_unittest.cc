// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor_login {

using password_manager::MockPasswordFormCache;
using password_manager::MockPasswordManager;
using password_manager::PasswordFormManager;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;
using testing::NiceMock;
using testing::Return;

namespace {

class FakePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  FakePasswordManagerClient() {
    profile_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>(
        password_manager::IsAccountStore(false));
    account_store_ = base::MakeRefCounted<password_manager::TestPasswordStore>(
        password_manager::IsAccountStore(true));
  }
  ~FakePasswordManagerClient() override = default;

  scoped_refptr<password_manager::TestPasswordStore> profile_store() {
    return profile_store_;
  }
  scoped_refptr<password_manager::TestPasswordStore> account_store() {
    return account_store_;
  }

 private:
  // PasswordManagerClient:
  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override {
    return profile_store_.get();
  }
  password_manager::PasswordStoreInterface* GetAccountPasswordStore()
      const override {
    return account_store_.get();
  }
  scoped_refptr<password_manager::TestPasswordStore> profile_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_store_;
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() = default;
  ~MockPasswordManagerDriver() override = default;

  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override));
};

}  // namespace

class ActorLoginDelegateImplTest : public ::testing::Test {
 public:
  ActorLoginDelegateImplTest() = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());

    delegate_ = static_cast<ActorLoginDelegateImpl*>(
        ActorLoginDelegateImpl::GetOrCreateForTesting(
            web_contents_, &client_,
            base::BindRepeating(
                [](MockPasswordManagerDriver* driver, content::WebContents*)
                    -> PasswordManagerDriver* { return driver; },
                base::Unretained(&mock_driver_))));

    client_.profile_store()->Init(profile_->GetPrefs(),
                                  /* affiliated_match_helper=*/nullptr);
    client_.account_store()->Init(profile_->GetPrefs(),
                                  /* affiliated_match_helper=*/nullptr);
  }

  void TearDown() override {
    client_.profile_store()->ShutdownOnUIThread();
    client_.account_store()->ShutdownOnUIThread();
  }

  void SetUpActorCredentialFillerDeps() {
    ON_CALL(mock_driver_, GetPasswordManager())
        .WillByDefault(Return(&mock_password_manager_));
    ON_CALL(mock_password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&mock_form_cache_));
    ON_CALL(mock_form_cache_, GetFormManagers())
        .WillByDefault(Return(base::span(form_managers_)));
  }

 protected:
  // Declare TaskEnvironment as the FIRST member to ensure proper lifetime.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;
  // `raw_ptr` because `TestWebContentsFactory` owns it
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  FakePasswordManagerClient client_;
  // `raw_ptr` because `WebContentsUserData` owns it
  raw_ptr<ActorLoginDelegateImpl> delegate_ = nullptr;
  NiceMock<MockPasswordManager> mock_password_manager_;
  NiceMock<MockPasswordFormCache> mock_form_cache_;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
  MockPasswordManagerDriver mock_driver_;
};

TEST_F(ActorLoginDelegateImplTest, GetCredentialsSuccess_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentials_FeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kActorLogin);
  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsServiceBusy) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  // Start the first request.
  base::test::TestFuture<CredentialsOrError> first_future;
  delegate_->GetCredentials(first_future.GetCallback());
  // Immediately try to start a second request, which should fail.
  base::test::TestFuture<CredentialsOrError> second_future;
  delegate_->GetCredentials(second_future.GetCallback());

  ASSERT_FALSE(second_future.Get().has_value());
  EXPECT_EQ(second_future.Get().error(), ActorLoginError::kServiceBusy);

  ASSERT_TRUE(first_future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, AttemptLogin_FeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kActorLogin);
  Credential credential =
      CreateTestCredential(u"username", GURL("https://example.com/login"));

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  // When the ActorLogin features is disabled, the delegate returns
  // `ActorLoginError::kUnknown`.
  EXPECT_EQ(future.Get().error(), ActorLoginError::kUnknown);
}

TEST_F(ActorLoginDelegateImplTest, AttemptLogin_FeatureOn) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  tabs::MockTabInterface mock_tab;
  Credential credential =
      CreateTestCredential(u"username", GURL("https://example.com/login"));

  MockPasswordManager mock_password_manager;
  MockPasswordFormCache mock_form_cache;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  EXPECT_CALL(mock_driver_, GetPasswordManager())
      .WillOnce(Return(&mock_password_manager));
  EXPECT_CALL(mock_password_manager, GetPasswordFormCache())
      .WillOnce(Return(&mock_form_cache));
  EXPECT_CALL(mock_form_cache, GetFormManagers())
      .WillOnce(Return(base::span(form_managers)));

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_F(ActorLoginDelegateImplTest, AttemptLoginServiceBusy_FeatureOn) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  Credential credential;

  SetUpActorCredentialFillerDeps();

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

  // Expect the first request to be answered.
  ASSERT_TRUE(first_future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, CallbacksAreResetAfterCompletion_FeatureOn) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  // First `GetCredentials` call.
  base::test::TestFuture<CredentialsOrError> future1;
  delegate_->GetCredentials(future1.GetCallback());
  ASSERT_TRUE(future1.Get().has_value());

  // Second `GetCredentials` call should now be possible.
  base::test::TestFuture<CredentialsOrError> future2;
  delegate_->GetCredentials(future2.GetCallback());
  ASSERT_TRUE(future2.Get().has_value());

  Credential credential;

  SetUpActorCredentialFillerDeps();

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
