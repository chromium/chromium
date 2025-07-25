// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace actor_login {
namespace {
using testing::Return;

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

}  // namespace

class ActorLoginDelegateImplTest : public ::testing::Test {
 public:
  ActorLoginDelegateImplTest() = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    delegate_ = ActorLoginDelegateImpl::GetOrCreateForWebContents(web_contents_,
                                                                  &client_);
    client_.profile_store()->Init(profile_->GetPrefs(),
                                  /* affiliated_match_helper=*/nullptr);
    client_.account_store()->Init(profile_->GetPrefs(),
                                  /* affiliated_match_helper=*/nullptr);
  }

  void TearDown() override {
    client_.profile_store()->ShutdownOnUIThread();
    client_.account_store()->ShutdownOnUIThread();
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

TEST_F(ActorLoginDelegateImplTest, GetCredentialsServiceBusy_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
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

TEST_F(ActorLoginDelegateImplTest, AttemptLoginSuccess_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  tabs::MockTabInterface mock_tab;
  Credential credential;

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  // Delegate's impl returns a default `LoginStatusResult`, which is false.
  EXPECT_FALSE(future.Get().value().value());
}

TEST_F(ActorLoginDelegateImplTest, AttemptLogin_FeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kActorLogin);
  Credential credential;

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kUnknown);
}

TEST_F(ActorLoginDelegateImplTest, AttemptLoginServiceBusy_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
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

TEST_F(ActorLoginDelegateImplTest, CallbacksAreResetAfterCompletion_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
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
  // First `AttemptLogin` call.
  base::test::TestFuture<LoginStatusResultOrError> future3;
  delegate_->AttemptLogin(credential, future3.GetCallback());
  ASSERT_TRUE(future3.Get().has_value());

  // Second `AttemptLogin` call should now be possible.
  base::test::TestFuture<LoginStatusResultOrError> future4;
  delegate_->AttemptLogin(credential, future4.GetCallback());
  ASSERT_TRUE(future4.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsFiltersByDomain_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);

  password_manager::PasswordForm form1;
  form1.url = GURL("https://foo.com");
  form1.signon_realm = form1.url.spec();
  form1.username_value = u"foo_username";
  form1.password_value = u"foo_password";
  client_.profile_store()->AddLogin(form1);

  password_manager::PasswordForm form2;
  form2.url = GURL("https://bar.com");
  form2.signon_realm = form2.url.spec();
  form2.username_value = u"bar_username";
  form2.password_value = u"bar_password";
  client_.account_store()->AddLogin(form2);

  content::WebContentsTester::For(web_contents_)
      ->SetLastCommittedURL(GURL("https://foo.com"));

  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  ASSERT_EQ(future.Get().value().size(), 1u);
  EXPECT_EQ(future.Get().value()[0].username, u"foo_username");
  EXPECT_EQ(future.Get().value()[0].type, kPassword);
  EXPECT_EQ(future.Get().value()[0].source_site_or_app, u"https://foo.com/");
  EXPECT_FALSE(future.Get().value()[0].immediatelyAvailableToLogin);
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsFromAllStores_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);

  password_manager::PasswordForm form1;
  form1.url = GURL("https://foo.com");
  form1.signon_realm = form1.url.spec();
  form1.username_value = u"foo_username";
  form1.password_value = u"foo_password";
  client_.profile_store()->AddLogin(form1);

  password_manager::PasswordForm form2;
  form2.url = GURL("https://foo.com");
  form2.signon_realm = form2.url.spec();
  form2.username_value = u"bar_username";
  form2.password_value = u"bar_password";
  client_.account_store()->AddLogin(form2);

  content::WebContentsTester::For(web_contents_)
      ->SetLastCommittedURL(GURL("https://foo.com"));

  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  const auto& credentials = future.Get().value();
  ASSERT_EQ(credentials.size(), 2u);

  std::vector<std::u16string> usernames;
  for (const auto& credential : credentials) {
    usernames.push_back(credential.username);
  }
  EXPECT_THAT(usernames,
              testing::UnorderedElementsAre(u"foo_username", u"bar_username"));
}

}  // namespace actor_login
