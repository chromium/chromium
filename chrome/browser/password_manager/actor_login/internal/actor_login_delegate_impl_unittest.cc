// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
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
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace actor_login {

using password_manager::MockPasswordFormCache;
using password_manager::MockPasswordManager;
using password_manager::PasswordFormManager;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;
using testing::NiceMock;
using testing::Return;

namespace {

constexpr char kTestUrl[] = "https://example.com/login";

class FakePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override, const));

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

class ActorLoginDelegateImplTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorLoginDelegateImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(contents.get(),
                                                               GURL(kTestUrl));

    delegate_ = static_cast<ActorLoginDelegateImpl*>(
        ActorLoginDelegateImpl::GetOrCreateForTesting(
            contents.get(), &client_,
            base::BindRepeating(
                [](MockPasswordManagerDriver* driver, content::WebContents*)
                    -> PasswordManagerDriver* { return driver; },
                base::Unretained(&mock_driver_))));

    client_.profile_store()->Init(/*affiliated_match_helper=*/nullptr);
    client_.account_store()->Init(/*affiliated_match_helper=*/nullptr);

    // Associate `contents` with a tab
    test_tab_strip_model_delegate_.SetBrowserWindowInterface(
        &mock_browser_window_interface_);
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &test_tab_strip_model_delegate_, profile());
    auto tab_model = std::make_unique<tabs::TabModel>(std::move(contents),
                                                      tab_strip_model_.get());
    tab_strip_model_->AppendTab(std::move(tab_model),
                                /*foreground=*/true);
    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(::testing::ReturnRef(user_data_host_));
  }

  void TearDown() override {
    client_.profile_store()->ShutdownOnUIThread();
    client_.account_store()->ShutdownOnUIThread();
    base::RunLoop().RunUntilIdle();

    // Reset the raw pointer before it becomes dangling in
    // ChromeRenderViewHostTestHarness::TearDown()
    delegate_ = nullptr;
    tab_strip_model_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetUpActorCredentialFillerDeps() {
    SetUpGetCredentialsDeps();
    ON_CALL(mock_password_manager_, GetClient())
        .WillByDefault(Return(&client_));
    ON_CALL(client_, GetPasswordManager)
        .WillByDefault(Return(&mock_password_manager_));
  }

  void SetUpGetCredentialsDeps() {
    ON_CALL(mock_driver_, GetPasswordManager())
        .WillByDefault(Return(&mock_password_manager_));
    ON_CALL(mock_password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&mock_form_cache_));
    ON_CALL(mock_form_cache_, GetFormManagers())
        .WillByDefault(Return(base::span(form_managers_)));
    ON_CALL(client_, GetPasswordManager)
        .WillByDefault(Return(&mock_password_manager_));
  }

 protected:
  FakePasswordManagerClient client_;
  // `raw_ptr` because `WebContentsUserData` owns it
  raw_ptr<ActorLoginDelegateImpl> delegate_ = nullptr;
  NiceMock<MockPasswordManager> mock_password_manager_;
  NiceMock<MockPasswordFormCache> mock_form_cache_;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
  MockPasswordManagerDriver mock_driver_;

  // Tab setup
  MockBrowserWindowInterface mock_browser_window_interface_;
  TestTabStripModelDelegate test_tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  ui::UnownedUserDataHost user_data_host_;
  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_tab_features_;
};

TEST_F(ActorLoginDelegateImplTest, GetCredentialsSuccess_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  SetUpGetCredentialsDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

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
  SetUpGetCredentialsDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

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
  Credential credential = CreateTestCredential(u"username", GURL(kTestUrl));

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  // When the ActorLogin features is disabled, the delegate returns
  // `ActorLoginError::kUnknown`.
  EXPECT_EQ(future.Get().error(), ActorLoginError::kUnknown);
}

TEST_F(ActorLoginDelegateImplTest, AttemptLogin_FeatureOn) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  Credential credential = CreateTestCredential(u"username", GURL(kTestUrl));

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_F(ActorLoginDelegateImplTest, AttemptLoginServiceBusy_FeatureOn) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  Credential credential = CreateTestCredential(u"username", GURL(kTestUrl));

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

  // Start the first request (`AttemptLogin`).
  base::test::TestFuture<LoginStatusResultOrError> first_future;
  delegate_->AttemptLogin(credential, false, first_future.GetCallback());
  // Immediately try to start a second request of the same type.
  base::test::TestFuture<LoginStatusResultOrError> second_future;
  delegate_->AttemptLogin(credential, false, second_future.GetCallback());

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
  SetUpActorCredentialFillerDeps();
  // Two calls to GetCredentials and two to AttemptLogin result
  // in four calls to GetFormManagers.
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(4);

  // First `GetCredentials` call.
  base::test::TestFuture<CredentialsOrError> future1;
  delegate_->GetCredentials(future1.GetCallback());
  ASSERT_TRUE(future1.Get().has_value());

  // Second `GetCredentials` call should now be possible.
  base::test::TestFuture<CredentialsOrError> future2;
  delegate_->GetCredentials(future2.GetCallback());
  ASSERT_TRUE(future2.Get().has_value());

  Credential credential = CreateTestCredential(u"username", GURL(kTestUrl));

  // First `AttemptLogin` call.
  base::test::TestFuture<LoginStatusResultOrError> future3;
  delegate_->AttemptLogin(credential, false, future3.GetCallback());
  ASSERT_TRUE(future3.Get().has_value());

  // Second `AttemptLogin` call should now be possible.
  base::test::TestFuture<LoginStatusResultOrError> future4;
  delegate_->AttemptLogin(credential, false, future4.GetCallback());
  ASSERT_TRUE(future4.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsAndAttemptLogin) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  Credential credential = CreateTestCredential(u"username", GURL(kTestUrl));

  SetUpActorCredentialFillerDeps();

  base::test::TestFuture<LoginStatusResultOrError> future;
  auto get_credentials_callback =
      base::BindLambdaForTesting([&](CredentialsOrError result) {
        ASSERT_TRUE(result.has_value());
        delegate_->AttemptLogin(credential, false, future.GetCallback());
      });

  delegate_->GetCredentials(get_credentials_callback);

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_F(ActorLoginDelegateImplTest,
       AttemptLoginLeavesServiceAvailableForSynchronousUse) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  Credential credential = CreateTestCredential(u"username", GURL(kTestUrl));

  SetUpActorCredentialFillerDeps();

  base::test::TestFuture<CredentialsOrError> future;
  delegate_->AttemptLogin(
      credential, false,
      base::BindLambdaForTesting([&](LoginStatusResultOrError result) {
        ASSERT_TRUE(result.has_value());
        delegate_->GetCredentials(future.GetCallback());
      }));
  ASSERT_TRUE(future.Get().has_value());
}

}  // namespace actor_login
