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
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_quality_logger.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
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
#include "url/gurl.h"
#include "url/origin.h"

namespace actor_login {

using autofill::FormData;
using device_reauth::DeviceAuthenticator;
using password_manager::FakeFormFetcher;
using password_manager::MockPasswordFormCache;
using password_manager::MockPasswordManager;
using password_manager::PasswordFormManager;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;
using password_manager::PasswordSaveManagerImpl;
using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::WithArg;

namespace {

constexpr char kTestUrl[] = "https://example.com/login";
constexpr char16_t kTestUsername[] = u"username";

template <bool success>
void PostResponse(base::OnceCallback<void(bool)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

class FakePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override, const));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (DeviceAuthenticator*),
              (override));

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
  MOCK_METHOD(const url::Origin&,
              GetLastCommittedOrigin,
              (),
              (const, override));

  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsNestedWithinFencedFrame, (), (const, override));
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override));
  MOCK_METHOD(void,
              CheckViewAreaVisible,
              (autofill::FieldRendererId, base::OnceCallback<void(bool)>),
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

  base::WeakPtr<MockActorLoginQualityLogger> mqls_logger() {
    return mock_mqls_logger.AsWeakPtr();
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

  std::unique_ptr<PasswordFormManager> CreateFormManagerWithParsedForm(
      const url::Origin& origin,
      const autofill::FormData& form_data,
      MockPasswordManagerDriver& mock_driver) {
    auto form_manager = std::make_unique<PasswordFormManager>(
        &client_, mock_driver.AsWeakPtr(), form_data, &form_fetcher_,
        std::make_unique<PasswordSaveManagerImpl>(&client_),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_manager->DisableFillingServerPredictionsForTesting();
    form_fetcher_.NotifyFetchCompleted();
    return form_manager;
  }

 protected:
  FakePasswordManagerClient client_;
  // `raw_ptr` because `WebContentsUserData` owns it
  raw_ptr<ActorLoginDelegateImpl> delegate_ = nullptr;
  NiceMock<MockPasswordManager> mock_password_manager_;
  NiceMock<MockPasswordFormCache> mock_form_cache_;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
  MockPasswordManagerDriver mock_driver_;
  FakeFormFetcher form_fetcher_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  MockActorLoginQualityLogger mock_mqls_logger;

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
  delegate_->GetCredentials(mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsLogsDomainAndLanguage) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  SetUpGetCredentialsDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

  const GURL kUrl = GURL("https://example.com");
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(kUrl);
  EXPECT_CALL(*mqls_logger(), SetDomainAndLanguage(_, Eq(kUrl)));
  delegate_->GetCredentials(mqls_logger(), base::DoNothing());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentials_FeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kActorLogin);
  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsServiceBusy) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

  // Start the first request.
  base::test::TestFuture<CredentialsOrError> first_future;
  delegate_->GetCredentials(mqls_logger(), first_future.GetCallback());
  // Immediately try to start a second request, which should fail.
  base::test::TestFuture<CredentialsOrError> second_future;
  delegate_->GetCredentials(mqls_logger(), second_future.GetCallback());

  ASSERT_FALSE(second_future.Get().has_value());
  EXPECT_EQ(second_future.Get().error(), ActorLoginError::kServiceBusy);

  ASSERT_TRUE(first_future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, AttemptLogin_FeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          future.GetCallback());

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), ActorLoginError::kFeatureDisabled);
}

TEST_F(ActorLoginDelegateImplTest, AttemptLogin_FeatureOn) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_F(ActorLoginDelegateImplTest, AttemptLoginLogsDomainAndLanguage) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL("https://example.com");
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();

  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(url);
  EXPECT_CALL(*mqls_logger(), SetDomainAndLanguage(_, Eq(url)));
  delegate_->AttemptLogin(credential, false, mqls_logger(), base::DoNothing());
}

TEST_F(ActorLoginDelegateImplTest, AttemptLoginServiceBusy_FeatureOn) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

  // Start the first request (`AttemptLogin`).
  base::test::TestFuture<LoginStatusResultOrError> first_future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          first_future.GetCallback());
  // Immediately try to start a second request of the same type.
  base::test::TestFuture<LoginStatusResultOrError> second_future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          second_future.GetCallback());

  // Immediately try to start a `GetCredentials` request (different type).
  base::test::TestFuture<CredentialsOrError> third_future;
  delegate_->GetCredentials(mqls_logger(), third_future.GetCallback());

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
  delegate_->GetCredentials(mqls_logger(), future1.GetCallback());
  ASSERT_TRUE(future1.Get().has_value());

  // Second `GetCredentials` call should now be possible.
  base::test::TestFuture<CredentialsOrError> future2;
  delegate_->GetCredentials(mqls_logger(), future2.GetCallback());
  ASSERT_TRUE(future2.Get().has_value());

  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  // First `AttemptLogin` call.
  base::test::TestFuture<LoginStatusResultOrError> future3;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          future3.GetCallback());
  ASSERT_TRUE(future3.Get().has_value());

  // Second `AttemptLogin` call should now be possible.
  base::test::TestFuture<LoginStatusResultOrError> future4;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          future4.GetCallback());
  ASSERT_TRUE(future4.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsAndAttemptLogin) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();

  base::test::TestFuture<LoginStatusResultOrError> future;
  auto get_credentials_callback =
      base::BindLambdaForTesting([&](CredentialsOrError result) {
        ASSERT_TRUE(result.has_value());
        delegate_->AttemptLogin(credential, false, mqls_logger(),
                                future.GetCallback());
      });

  delegate_->GetCredentials(mqls_logger(), get_credentials_callback);

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(), LoginStatusResult::kErrorNoSigninForm);
}

TEST_F(ActorLoginDelegateImplTest,
       AttemptLoginLeavesServiceAvailableForSynchronousUse) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();

  base::test::TestFuture<CredentialsOrError> future;
  delegate_->AttemptLogin(
      credential, false, mqls_logger(),
      base::BindLambdaForTesting([&](LoginStatusResultOrError result) {
        ASSERT_TRUE(result.has_value());
        delegate_->GetCredentials(mqls_logger(), future.GetCallback());
      }));
  ASSERT_TRUE(future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, WebContentsDestroyedDuringAttemptLogin) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers()).Times(1);

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          future.GetCallback());

  delegate_ = nullptr;
  // This should invoke `WebContentsDestroyed`.
  tab_strip_model_.reset();
  task_environment()->RunUntilIdle();
  // The callback should never be invoked because the
  // delegate was destroyed.
  EXPECT_FALSE(future.IsReady());
}

// If the window is not active and reauth before filling is required,
// `AttemptLogin` should return LoginStatusResult::kErrorDeviceReauthRequired.
TEST_F(ActorLoginDelegateImplTest, FillingReauthRequiredWindowNotActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(/*enabled_features=*/
                                {password_manager::features::kActorLogin,
                                 password_manager::features::
                                     kActorLoginReauthTaskRefocus},
                                /*disabled_features=*/{});
  const url::Origin origin = url::Origin::Create(GURL(kTestUrl));
  const Credential credential =
      CreateTestCredential(kTestUsername, origin.GetURL(), origin);
  const FormData form_data = CreateSigninFormData(origin.GetURL());
  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(
      CreateSavedPasswordForm(origin.GetURL(), kTestUsername));
  form_fetcher_.SetBestMatches(saved_forms);

  // Make sure that all the conditions for filling are fulfilled.
  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));

  std::vector<std::unique_ptr<PasswordFormManager>> form_managers;
  form_managers.push_back(
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_));

  SetUpActorCredentialFillerDeps();

  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers)));
  EXPECT_CALL(client_, IsReauthBeforeFillingRequired).WillOnce(Return(true));

  EXPECT_CALL(mock_browser_window_interface_, IsActive).WillOnce(Return(false));

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(),
            LoginStatusResult::kErrorDeviceReauthRequired);
}

}  // namespace actor_login
