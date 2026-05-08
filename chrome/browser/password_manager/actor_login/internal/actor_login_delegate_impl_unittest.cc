// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_cleaning_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_service_factory.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_metrics_helper.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_cleaning_service.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_permission_service.h"
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
#include "components/tabs/public/tab_interface.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
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
using testing::An;
using testing::Eq;
using testing::NiceMock;
using testing::Optional;
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

class MockActorLoginPermissionCleaningService
    : public ActorLoginPermissionCleaningService {
 public:
  MockActorLoginPermissionCleaningService() = default;
  ~MockActorLoginPermissionCleaningService() override = default;
  MOCK_METHOD(void,
              ClearConflictingPermissions,
              (const Credential& credential,
               bool check_federated_credentials,
               base::OnceClosure done_callback),
              (override));
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
  MOCK_METHOD(void,
              FillField,
              (autofill::FieldRendererId,
               const std::u16string&,
               autofill::FieldPropertiesFlags,
               base::OnceCallback<void(bool)>),
              (override));
};

class MockActionSequenceDelegate : public ActionSequenceDelegate {
 public:
  MockActionSequenceDelegate() = default;
  ~MockActionSequenceDelegate() override = default;
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActionSequenceEnded,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void, OnFederatedLoginOutcome, (LoginStatusResult), (override));
};

}  // namespace

class ActorLoginDelegateImplTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorLoginDelegateImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI) {
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(IS_ANDROID)
    disabled_features.push_back(
        password_manager::features::kActorLoginNoPermanentPermissionsAndroid);
#endif
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicActor},
        /*disabled_features=*/disabled_features);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ON_CALL(mock_driver_, GetLastCommittedOrigin())
        .WillByDefault(ReturnRef(test_origin_));

    ActorLoginPermissionServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          auto mock_service =
              std::make_unique<NiceMock<MockActorLoginPermissionService>>();
          ON_CALL(*mock_service, ListPermissions(An<const url::Origin&>(), _))
              .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
                  std::vector<FederatedPermission>()));
          return mock_service;
        }));

    web_contents_ = CreateTestWebContents();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents_.get(), GURL(kTestUrl));

    mock_tab_interface_ = std::make_unique<tabs::MockTabInterface>();
    tabs::TabLookupFromWebContents::CreateForWebContents(
        web_contents_.get(), mock_tab_interface_.get());

    ON_CALL(*mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_interface_));
    ON_CALL(*mock_tab_interface_, IsActivated).WillByDefault(Return(true));

    delegate_ = static_cast<ActorLoginDelegateImpl*>(
        ActorLoginDelegateImpl::GetOrCreateForTesting(
            web_contents_.get(), &client_,
            base::BindRepeating(
                [](MockPasswordManagerDriver* driver, content::WebContents*)
                    -> PasswordManagerDriver* { return driver; },
                base::Unretained(&mock_driver_))));

    client_.profile_store()->Init(/*affiliated_match_helper=*/nullptr);
    client_.account_store()->Init(/*affiliated_match_helper=*/nullptr);

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
    web_contents_.reset();
    mock_tab_interface_.reset();
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

  void SetUpConflictingPermissions(
      const GURL& url,
      const std::u16string& picked_credential_username) {
    // Set up two credentials with permission to trigger conflicting permissions
    // in `GetCredentials`.
    std::vector<password_manager::PasswordForm> saved_forms;
    password_manager::PasswordForm form1 =
        CreateSavedPasswordForm(url, picked_credential_username);
    form1.actor_login_approved = true;
    saved_forms.push_back(form1);

    password_manager::PasswordForm form2 =
        CreateSavedPasswordForm(url, u"user2");
    form2.actor_login_approved = true;
    saved_forms.push_back(form2);

    form_fetcher_.SetBestMatches(saved_forms);
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
  // Needs to be declared before `form_managers_` to avoid use-after-free.
  // `form_managers_` holds a vector of `PasswordFormManager` instances, which
  // hold a pointer to `form_fetcher_`.
  FakeFormFetcher form_fetcher_;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
  NiceMock<MockPasswordManagerDriver> mock_driver_;
  url::Origin test_origin_ = url::Origin::Create(GURL(kTestUrl));
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_{
      {.disable_server_communication = true}};
  NiceMock<MockActorLoginQualityLogger> mock_mqls_logger;

  // Tab setup
  NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_interface_;
  ui::UnownedUserDataHost user_data_host_;
};

TEST_F(ActorLoginDelegateImplTest, GetCredentialsSuccess_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  SetUpGetCredentialsDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsLogsDomainAndLanguage) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  SetUpGetCredentialsDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  const GURL kUrl = GURL("https://example.com");
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(kUrl);
  EXPECT_CALL(*mqls_logger(), SetDomainAndLanguage(_, Eq(kUrl)));
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), base::DoNothing());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentials_FeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kActorLogin);
  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value().empty());
}

TEST_F(ActorLoginDelegateImplTest, GetCredentialsServiceBusy) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kActorLogin);
  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  // Start the first request.
  base::test::TestFuture<CredentialsOrError> first_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), first_future.GetCallback());
  // Immediately try to start a second request, which should fail.
  base::test::TestFuture<CredentialsOrError> second_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), second_future.GetCallback());

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
                          base::TimeTicks::Now(), future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

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
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

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

  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(url);
  EXPECT_CALL(*mqls_logger(), SetDomainAndLanguage(_, Eq(url)));
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), base::DoNothing(),
                          /*action_sequence_delegate=*/nullptr);
}

TEST_F(ActorLoginDelegateImplTest, AttemptLoginServiceBusy_FeatureOn) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  // Start the first request (`AttemptLogin`).
  base::test::TestFuture<LoginStatusResultOrError> first_future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), first_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);
  // Immediately try to start a second request of the same type.
  base::test::TestFuture<LoginStatusResultOrError> second_future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), second_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  // Immediately try to start a `GetCredentials` request (different type).
  base::test::TestFuture<CredentialsOrError> third_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), third_future.GetCallback());

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
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), future1.GetCallback());
  ASSERT_TRUE(future1.Get().has_value());

  // Second `GetCredentials` call should now be possible.
  base::test::TestFuture<CredentialsOrError> future2;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), future2.GetCallback());
  ASSERT_TRUE(future2.Get().has_value());

  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  // First `AttemptLogin` call.
  base::test::TestFuture<LoginStatusResultOrError> future3;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), future3.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);
  ASSERT_TRUE(future3.Get().has_value());

  // Second `AttemptLogin` call should now be possible.
  base::test::TestFuture<LoginStatusResultOrError> future4;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), future4.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);
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
                                base::TimeTicks::Now(), future.GetCallback(),
                                /*action_sequence_delegate=*/nullptr);
      });

  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_credentials_callback);

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
      credential, false, mqls_logger(), base::TimeTicks::Now(),
      base::BindLambdaForTesting([&](LoginStatusResultOrError result) {
        ASSERT_TRUE(result.has_value());
        delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                                  mqls_logger(), future.GetCallback());
      }),
      /*action_sequence_delegate=*/nullptr);
  ASSERT_TRUE(future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, WebContentsDestroyedDuringAttemptLogin) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  delegate_ = nullptr;
  // This should invoke `WebContentsDestroyed`.
  web_contents_.reset();
  task_environment()->RunUntilIdle();
  // The callback should never be invoked because the
  // delegate was destroyed.
  EXPECT_FALSE(future.IsReady());
}

#if !BUILDFLAG(IS_ANDROID)
// If the window is not active and reauth before filling is required,
// `AttemptLogin` should return LoginStatusResult::kErrorDeviceReauthRequired.
TEST_F(ActorLoginDelegateImplTest, FillingReauthRequiredWindowNotActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(/*enabled_features=*/
                                {password_manager::features::kActorLogin},
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
                          base::TimeTicks::Now(), future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(),
            LoginStatusResult::kErrorDeviceReauthRequired);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ActorLoginDelegateImplTest, RecordActorLoginMetricsNoCredentials) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetUpActorCredentialFillerDeps();

  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());
  ASSERT_TRUE(get_creds_future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "Actor.Login.AccountTypesShown",
      static_cast<int>(ActorLoginAccountTypes::kNone), 1);
  histogram_tester.ExpectUniqueSample("Actor.Login.NumAccountsShown", 0, 1);
  histogram_tester.ExpectTotalCount("Actor.Login.GetCredentialsLatency", 1);

  // UKM should be recorded because no credentials were returned and the
  // delegate resets metrics_helper_ in that case.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Actor_Login::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kAccountTypesShownName,
      static_cast<int>(ActorLoginAccountTypes::kNone));
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kNumAccountsShownName, 0);
}

TEST_F(ActorLoginDelegateImplTest,
       RecordActorLoginMetricsWithCredentialsNotShown) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);

  SetUpActorCredentialFillerDeps();

  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());

  // Currently no sign in form means the account is not displayed.
  histogram_tester.ExpectBucketCount(
      "Actor.Login.AccountTypesShown",
      static_cast<int>(ActorLoginAccountTypes::kNone), 1);
  histogram_tester.ExpectBucketCount("Actor.Login.NumAccountsShown", 0, 1);

  // UKM should be recorded because no credentials were could be displayed.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Actor_Login::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kAccountTypesShownName,
      static_cast<int>(ActorLoginAccountTypes::kNone));
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kNumAccountsShownName, 0);
}

TEST_F(ActorLoginDelegateImplTest, RecordActorLoginMetricsOnAttemptLogin) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);
  credential.type = CredentialType::kPassword;
  credential.has_persistent_permission = true;

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  ASSERT_TRUE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "Actor.Login.SelectedAccountType",
      static_cast<int>(ActorLoginSelectedAccountType::kPassword), 1);
  histogram_tester.ExpectUniqueSample("Actor.Login.AccountAutoSelected", true,
                                      1);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Actor_Login::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kSelectedAccountTypeName,
      static_cast<int>(ActorLoginSelectedAccountType::kPassword));
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kAccountAutoSelectedName, true);
}

TEST_F(ActorLoginDelegateImplTest,
       RecordActorLoginMetricsOnFederatedAttemptLogin) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  content::WebContents* test_contents = web_contents_.get();
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);
  credential.type = CredentialType::kFederated;
  FederationDetail& federation_detail = credential.federation_detail.emplace();
  federation_detail.idp_origin =
      url::Origin::Create(GURL("https://accounts.google.com"));
  federation_detail.account_id = "12345";

  SetUpActorCredentialFillerDeps();

  // Create a task and associate it with the tab. Avoids hitting a CHECK when
  // invoking `GetCredentials`.
  actor::ActorKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return std::make_unique<actor::ActorKeyedServiceFake>(
            Profile::FromBrowserContext(context));
      }));
  auto* actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedService::Get(profile()));
  actor::TaskId task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  base::RunLoop loop;
  task->AddTab(tabs::TabInterface::GetFromContents(test_contents)->GetHandle(),
               /*stop_task_on_detach=*/true,
               base::BindLambdaForTesting(
                   [&](actor::mojom::ActionResultPtr result) { loop.Quit(); }));
  loop.Run();

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  // Trigger completion for federated login.
  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(test_contents);
  ASSERT_TRUE(request);
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kSuccess);

  ASSERT_TRUE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "Actor.Login.SelectedAccountType",
      static_cast<int>(ActorLoginSelectedAccountType::kFederated), 1);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Actor_Login::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kSelectedAccountTypeName,
      static_cast<int>(ActorLoginSelectedAccountType::kFederated));
}

TEST_F(ActorLoginDelegateImplTest,
       RecordActorLoginMetricsGetCredentialsAndAttemptLogin) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());
  ASSERT_TRUE(get_creds_future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "Actor.Login.AccountTypesShown",
      static_cast<int>(ActorLoginAccountTypes::kNone), 1);
  histogram_tester.ExpectUniqueSample("Actor.Login.NumAccountsShown", 0, 1);
  histogram_tester.ExpectTotalCount("Actor.Login.GetCredentialsLatency", 1);

  // UKM should be recorded because no credentials were returned.
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Actor_Login::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kAccountTypesShownName,
      static_cast<int>(ActorLoginAccountTypes::kNone));
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_Login::kNumAccountsShownName, 0);

  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);
  credential.has_persistent_permission = true;

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);
  ASSERT_TRUE(attempt_login_future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Actor.Login.SelectedAccountType",
      static_cast<int>(ActorLoginSelectedAccountType::kPassword), 1);
  histogram_tester.ExpectUniqueSample("Actor.Login.AccountAutoSelected", 1, 1);

  entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Actor_Login::kEntryName);
  ASSERT_EQ(2u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[1], ukm::builders::Actor_Login::kSelectedAccountTypeName,
      static_cast<int>(ActorLoginSelectedAccountType::kPassword));
  ukm_recorder.ExpectEntryMetric(
      entries[1], ukm::builders::Actor_Login::kAccountAutoSelectedName, true);
}

TEST_F(ActorLoginDelegateImplTest,
       AttemptLoginRegistersObserverAndTriggersCleanup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{password_manager::features::kActorLogin,
                            password_manager::features::
                                kActorLoginConflictingPermissionCleanup},
      /*disabled_features=*/{});
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  SetUpConflictingPermissions(url, kTestUsername);

  // Set up a signin form on the page to force the getter into calling
  // the hooked fake fetcher.
  const autofill::FormData form_data = CreateSigninFormData(url);
  form_managers_.push_back(
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_));

  SetUpActorCredentialFillerDeps();
  // Mock driver methods that are called by `GetCredentials` to check
  // whether there is a signin form on the page.
  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));
  SetUpActorCredentialFillerDeps();

  SetUpConflictingPermissions(url, kTestUsername);

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  // Call `GetCredentials` first to find conflicting permissions.
  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());

  // Wait until the async visibility checks complete and the fetcher registers
  // as a consumer.
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return form_fetcher_.HasConsumers(); }));

  form_fetcher_.NotifyFetchCompleted();
  ASSERT_TRUE(get_creds_future.Get().has_value());

  EXPECT_CALL(mock_driver_, FillField)
      .WillRepeatedly(WithArg<3>(&PostResponse<true>));

  Credential credential = CreateTestCredential(kTestUsername, url, origin);
  credential.type = CredentialType::kPassword;
  credential.source_site_or_app =
      base::UTF8ToUTF16(url.GetWithEmptyPath().spec());

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/true,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);
  EXPECT_CALL(mock_password_manager_, AddObserver(delegate_.get()));
  ASSERT_TRUE(attempt_login_future.Wait());

  // Now simulate a successful login notification.
  password_manager::PasswordForm form;
  form.url = url;
  form.signon_realm = base::UTF16ToUTF8(credential.source_site_or_app);
  form.username_value = kTestUsername;
  form.actor_login_approved = true;

  EXPECT_CALL(mock_password_manager_, RemoveObserver(delegate_.get()));

  auto* cleaning_service =
      static_cast<MockActorLoginPermissionCleaningService*>(
          ActorLoginPermissionCleaningServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<
                        NiceMock<MockActorLoginPermissionCleaningService>>();
                  })));

  EXPECT_CALL(*cleaning_service,
              ClearConflictingPermissions(Eq(credential), _, _));
  delegate_->OnLoginSuccessful(form);
}

TEST_F(ActorLoginDelegateImplTest,
       OnLoginSuccessful_MismatchedUsername_NoCleanup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{password_manager::features::kActorLogin,
                            password_manager::features::
                                kActorLoginConflictingPermissionCleanup},
      /*disabled_features=*/{});
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(kTestUsername, url, origin);
  credential.type = CredentialType::kPassword;
  credential.source_site_or_app =
      base::UTF8ToUTF16(url.GetWithEmptyPath().spec());

  SetUpConflictingPermissions(url, kTestUsername);

  // Set up a signin form on the page to force the getter into calling
  // the hooked fake fetcher.
  const autofill::FormData form_data = CreateSigninFormData(url);
  form_managers_.push_back(
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_));

  SetUpActorCredentialFillerDeps();

  // Mock driver methods that are called by `GetCredentials` to check
  // whether there is a signin form on the page.
  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  // Call `GetCredentials` first to find conflicting permissions.
  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());

  // Wait until the async visibility checks complete and the fetcher registers
  // as a consumer.
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return form_fetcher_.HasConsumers(); }));

  form_fetcher_.NotifyFetchCompleted();
  ASSERT_TRUE(get_creds_future.Get().has_value());

  EXPECT_CALL(mock_driver_, FillField)
      .WillRepeatedly(WithArg<3>(&PostResponse<true>));
  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/true,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  EXPECT_CALL(mock_password_manager_, AddObserver(delegate_.get()));
  ASSERT_TRUE(attempt_login_future.Wait());

  // Simulate a successful login notification with a DIFFERENT username.
  password_manager::PasswordForm form;
  form.url = url;
  form.username_value = u"wrong_username";
  form.actor_login_approved = true;

  auto* cleaning_service =
      static_cast<MockActorLoginPermissionCleaningService*>(
          ActorLoginPermissionCleaningServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<
                        NiceMock<MockActorLoginPermissionCleaningService>>();
                  })));

  EXPECT_CALL(*cleaning_service, ClearConflictingPermissions).Times(0);

  delegate_->OnLoginSuccessful(form);
}

TEST_F(ActorLoginDelegateImplTest,
       OnLoginSuccessful_MismatchedRealm_NoCleanup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{password_manager::features::kActorLogin,
                            password_manager::features::
                                kActorLoginConflictingPermissionCleanup},
      /*disabled_features=*/{});

  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  SetUpConflictingPermissions(url, kTestUsername);
  // Set up a signin form on the page to force the getter into calling
  // the hooked fake fetcher.
  const autofill::FormData form_data = CreateSigninFormData(url);
  form_managers_.push_back(
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_));

  // Mock driver methods that are called by `GetCredentials` to check
  // whether there is a signin form on the page.
  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));
  SetUpActorCredentialFillerDeps();

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  // Call `GetCredentials` first to find conflicting permissions.
  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());

  // Wait until the async visibility checks complete and the fetcher registers
  // as a consumer.
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return form_fetcher_.HasConsumers(); }));

  form_fetcher_.NotifyFetchCompleted();
  ASSERT_TRUE(get_creds_future.Get().has_value());

  EXPECT_CALL(mock_driver_, FillField)
      .WillRepeatedly(WithArg<3>(&PostResponse<true>));

  Credential credential = CreateTestCredential(u"username", url, origin);
  credential.type = CredentialType::kPassword;
  credential.source_site_or_app =
      base::UTF8ToUTF16(url.GetWithEmptyPath().spec());

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/true,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);
  EXPECT_CALL(mock_password_manager_, AddObserver(delegate_.get()));
  ASSERT_TRUE(attempt_login_future.Wait());

  auto* cleaning_service =
      static_cast<MockActorLoginPermissionCleaningService*>(
          ActorLoginPermissionCleaningServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<
                        NiceMock<MockActorLoginPermissionCleaningService>>();
                  })));

  EXPECT_CALL(*cleaning_service, ClearConflictingPermissions).Times(0);

  password_manager::PasswordForm form;
  form.url = GURL("https://some-other-site.com/");
  form.username_value = kTestUsername;
  form.actor_login_approved = true;
  delegate_->OnLoginSuccessful(form);
}

TEST_F(ActorLoginDelegateImplTest,
       AttemptLoginWithNoConflictingPermissionsDoesNotRegisterObserver) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);

  // Set up a signin form on the page.
  const autofill::FormData form_data = CreateSigninFormData(url);
  auto form_manager =
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_);
  form_managers_.push_back(std::move(form_manager));

  SetUpActorCredentialFillerDeps();

  // Make sure that all conditions for the form to fill are fulfilled.
  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  EXPECT_CALL(mock_driver_, FillField)
      .WillRepeatedly(WithArg<3>(&PostResponse<true>));

  Credential credential = CreateTestCredential(u"username", url, origin);
  credential.type = CredentialType::kPassword;
  credential.source_site_or_app =
      base::UTF8ToUTF16(url.GetWithEmptyPath().spec());

  // Expect that AddObserver is NOT called.
  EXPECT_CALL(mock_password_manager_, AddObserver).Times(0);

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/true,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  ASSERT_TRUE(attempt_login_future.Wait());
}

TEST_F(
    ActorLoginDelegateImplTest,
    AttemptLoginWithConflictingPermissionsAndNoStorePermissionDoesNotRegisterObserver) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{password_manager::features::kActorLogin,
                            password_manager::features::
                                kActorLoginConflictingPermissionCleanup},
      /*disabled_features=*/{});
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  SetUpConflictingPermissions(url, kTestUsername);

  // Set up a signin form on the page.
  const autofill::FormData form_data = CreateSigninFormData(url);
  auto form_manager =
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_);
  form_managers_.push_back(std::move(form_manager));

  SetUpActorCredentialFillerDeps();

  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());

  ASSERT_TRUE(
      base::test::RunUntil([this]() { return form_fetcher_.HasConsumers(); }));

  form_fetcher_.NotifyFetchCompleted();
  ASSERT_TRUE(get_creds_future.Get().has_value());

  EXPECT_CALL(mock_driver_, FillField)
      .WillRepeatedly(WithArg<3>(&PostResponse<true>));

  Credential credential = CreateTestCredential(kTestUsername, url, origin);
  credential.type = CredentialType::kPassword;
  credential.source_site_or_app =
      base::UTF8ToUTF16(url.GetWithEmptyPath().spec());

  EXPECT_CALL(mock_password_manager_, AddObserver).Times(0);

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/false,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  ASSERT_TRUE(attempt_login_future.Wait());
}

TEST_F(
    ActorLoginDelegateImplTest,
    AttemptLoginWithConflictingPermissionsAndFillingFailureDoesNotRegisterObserver) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{password_manager::features::kActorLogin,
                            password_manager::features::
                                kActorLoginConflictingPermissionCleanup},
      /*disabled_features=*/{});
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  SetUpConflictingPermissions(url, kTestUsername);

  // Set up a signin form on the page.
  const autofill::FormData form_data = CreateSigninFormData(url);
  auto form_manager =
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_);
  form_managers_.push_back(std::move(form_manager));

  SetUpActorCredentialFillerDeps();

  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());

  ASSERT_TRUE(
      base::test::RunUntil([this]() { return form_fetcher_.HasConsumers(); }));

  form_fetcher_.NotifyFetchCompleted();
  ASSERT_TRUE(get_creds_future.Get().has_value());

  // Make FillField return false (failure).
  EXPECT_CALL(mock_driver_, FillField)
      .WillRepeatedly(WithArg<3>(&PostResponse<false>));

  Credential credential = CreateTestCredential(kTestUsername, url, origin);
  credential.type = CredentialType::kPassword;
  credential.source_site_or_app =
      base::UTF8ToUTF16(url.GetWithEmptyPath().spec());

  // Expect that AddObserver is NOT called.
  EXPECT_CALL(mock_password_manager_, AddObserver).Times(0);

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/true,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  ASSERT_TRUE(attempt_login_future.Wait());
}

TEST_F(ActorLoginDelegateImplTest,
       GetCredentialsWithSiwgButtonFetchesFederated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kActorLogin,
       ::features::kFedCmEmbedderInitiatedLogin},
      {});
  SetUpGetCredentialsDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  auto* mock_permission_service = static_cast<MockActorLoginPermissionService*>(
      ActorLoginPermissionServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*mock_permission_service,
              ListPermissions(An<const url::Origin&>(), _));

  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/true,
                            mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest,
       GetCredentialsWithoutSiwgButtonSkipsFederated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kActorLogin,
       ::features::kFedCmEmbedderInitiatedLogin},
      {});
  SetUpGetCredentialsDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers());

  auto* mock_permission_service = static_cast<MockActorLoginPermissionService*>(
      ActorLoginPermissionServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*mock_permission_service,
              ListPermissions(An<const url::Origin&>(), _))
      .Times(0);

  base::test::TestFuture<CredentialsOrError> future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
}

TEST_F(ActorLoginDelegateImplTest, RemovedOnUserTakeover) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);

  SetUpGetCredentialsDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  SetUpActorCredentialFillerDeps();
  actor::ActorKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return std::make_unique<actor::ActorKeyedServiceFake>(
            Profile::FromBrowserContext(context));
      }));

  auto* actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedService::Get(profile()));

  // Create a task and associate it with the tab.
  actor::TaskId task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  content::WebContents* test_contents = web_contents_.get();
  base::RunLoop loop;
  task->AddTab(tabs::TabInterface::GetFromContents(test_contents)->GetHandle(),
               /*stop_task_on_detach=*/true,
               base::BindLambdaForTesting(
                   [&](actor::mojom::ActionResultPtr result) { loop.Quit(); }));
  loop.Run();

  // Invoke AttemptLogin with a federated credential
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  Credential credential = CreateTestCredential(u"username", url, origin);
  credential.type = CredentialType::kFederated;
  FederationDetail& federation_detail = credential.federation_detail.emplace();
  federation_detail.idp_origin =
      url::Origin::Create(GURL("https://accounts.google.com"));
  federation_detail.account_id = "12345";

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/false,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  // Check that a FederatedEmbedderLoginRequest was set.
  EXPECT_NE(nullptr,
            content::webid::FederatedEmbedderLoginRequest::Get(test_contents));

  // Stop the task, which should invoke the callback.
  actor_service->StopTaskForTesting(
      task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
  ASSERT_TRUE(attempt_login_future.Wait());

  // Verify that the FederatedEmbedderLoginRequest is no longer set.
  EXPECT_EQ(nullptr,
            content::webid::FederatedEmbedderLoginRequest::Get(test_contents));
}

TEST_F(ActorLoginDelegateImplTest,
       SuccessfulContinuationClearsConflictingPermissions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {password_manager::features::kActorLogin,
       password_manager::features::kActorLoginConflictingPermissionCleanup},
      /*disabled_features=*/{});

  // Setup mock cleaning service
  auto* mock_cleaning_service =
      static_cast<MockActorLoginPermissionCleaningService*>(
          ActorLoginPermissionCleaningServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<
                        NiceMock<MockActorLoginPermissionCleaningService>>();
                  })));

  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  SetUpConflictingPermissions(url, kTestUsername);

  // Set up a signin form on the page to force the getter into calling
  // the hooked fake fetcher.
  const autofill::FormData form_data = CreateSigninFormData(url);
  form_managers_.push_back(
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_));

  // Mock driver methods that are called by `GetCredentials` to check
  // whether there is a signin form on the page.
  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));
  SetUpActorCredentialFillerDeps();

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  // Call `GetCredentials` first to find conflicting permissions.
  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());

  // Wait until the async visibility checks complete and the fetcher registers
  // as a consumer.
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return form_fetcher_.HasConsumers(); }));

  form_fetcher_.NotifyFetchCompleted();
  ASSERT_TRUE(get_creds_future.Get().has_value());

  // Setup ActorKeyedServiceFake
  auto* actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
            return std::make_unique<actor::ActorKeyedServiceFake>(
                Profile::FromBrowserContext(context));
          })));

  actor::TaskId task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  content::WebContents* test_contents = web_contents_.get();
  base::RunLoop loop;
  task->AddTab(tabs::TabInterface::GetFromContents(test_contents)->GetHandle(),
               /*stop_task_on_detach=*/true,
               base::BindLambdaForTesting(
                   [&](actor::mojom::ActionResultPtr result) { loop.Quit(); }));
  loop.Run();
  Credential credential = CreateTestCredential(kTestUsername, url, origin);
  credential.type = CredentialType::kFederated;
  FederationDetail federation_detail;
  federation_detail.idp_origin =
      url::Origin::Create(GURL("https://accounts.google.com"));
  federation_detail.account_id = "12345";
  credential.federation_detail = federation_detail;

  MockActionSequenceDelegate mock_action_delegate;
  base::WeakPtrFactory<ActionSequenceDelegate> factory(&mock_action_delegate);

  EXPECT_CALL(mock_action_delegate, RegisterActionSequenceEnded)
      .WillOnce([&](base::OnceCallback<void(bool)> callback) {
        return base::CallbackListSubscription();
      });

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/true,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          factory.GetWeakPtr());

  ASSERT_TRUE(attempt_login_future.Wait());
  ASSERT_TRUE(attempt_login_future.Get().has_value());
  EXPECT_EQ(attempt_login_future.Get().value(),
            LoginStatusResult::kRequiresButtonClick);

  auto* request = content::webid::FederatedEmbedderLoginRequest::Get(
      delegate_->web_contents());
  ASSERT_TRUE(request);
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);

  EXPECT_CALL(*mock_cleaning_service,
              ClearConflictingPermissions(Eq(credential), _, _));

  static_cast<content::WebContentsObserver*>(delegate_->siwg_controller())
      ->OnFedCmFederatedLogin(true);
}

TEST_F(ActorLoginDelegateImplTest, FailedFederatedLoginDoesntClearPermissions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {password_manager::features::kActorLogin,
       password_manager::features::kActorLoginConflictingPermissionCleanup},
      /*disabled_features=*/{});

  // Setup mock cleaning service
  auto* mock_cleaning_service =
      static_cast<MockActorLoginPermissionCleaningService*>(
          ActorLoginPermissionCleaningServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<
                        NiceMock<MockActorLoginPermissionCleaningService>>();
                  })));

  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  SetUpConflictingPermissions(url, kTestUsername);

  // Set up a signin form on the page to force the getter into calling
  // the hooked fake fetcher.
  const autofill::FormData form_data = CreateSigninFormData(url);
  form_managers_.push_back(
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_));

  // Mock driver methods that are called by `GetCredentials` to check
  // whether there is a signin form on the page.
  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));
  ON_CALL(mock_driver_, CheckViewAreaVisible)
      .WillByDefault(WithArg<1>(&PostResponse<true>));
  SetUpActorCredentialFillerDeps();

  EXPECT_CALL(mock_form_cache_, GetFormManagers())
      .WillRepeatedly(Return(base::span(form_managers_)));

  // Call `GetCredentials` first to find conflicting permissions.
  base::test::TestFuture<CredentialsOrError> get_creds_future;
  delegate_->GetCredentials(/*has_sign_in_with_google_button=*/false,
                            mqls_logger(), get_creds_future.GetCallback());

  // Wait until the async visibility checks complete and the fetcher registers
  // as a consumer.
  ASSERT_TRUE(
      base::test::RunUntil([this]() { return form_fetcher_.HasConsumers(); }));

  form_fetcher_.NotifyFetchCompleted();
  ASSERT_TRUE(get_creds_future.Get().has_value());

  // Setup ActorKeyedServiceFake
  auto* actor_service = static_cast<actor::ActorKeyedServiceFake*>(
      actor::ActorKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
            return std::make_unique<actor::ActorKeyedServiceFake>(
                Profile::FromBrowserContext(context));
          })));

  actor::TaskId task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  content::WebContents* test_contents = web_contents_.get();
  base::RunLoop loop;
  task->AddTab(tabs::TabInterface::GetFromContents(test_contents)->GetHandle(),
               /*stop_task_on_detach=*/true,
               base::BindLambdaForTesting(
                   [&](actor::mojom::ActionResultPtr result) { loop.Quit(); }));
  loop.Run();

  Credential credential = CreateTestCredential(u"username", url, origin);
  credential.type = CredentialType::kFederated;
  FederationDetail federation_detail;
  federation_detail.idp_origin =
      url::Origin::Create(GURL("https://accounts.google.com"));
  federation_detail.account_id = "12345";
  credential.federation_detail = federation_detail;

  MockActionSequenceDelegate mock_action_delegate;
  base::WeakPtrFactory<ActionSequenceDelegate> factory(&mock_action_delegate);
  base::OnceCallback<void(bool)> captured_callback;

  EXPECT_CALL(mock_action_delegate, RegisterActionSequenceEnded)
      .WillOnce([&](base::OnceCallback<void(bool)> callback) {
        captured_callback = std::move(callback);
        return base::CallbackListSubscription();
      });

  base::test::TestFuture<LoginStatusResultOrError> attempt_login_future;
  delegate_->AttemptLogin(credential, /*should_store_permission=*/true,
                          mqls_logger(), base::TimeTicks::Now(),
                          attempt_login_future.GetCallback(),
                          factory.GetWeakPtr());

  ASSERT_TRUE(attempt_login_future.Wait());
  ASSERT_TRUE(attempt_login_future.Get().has_value());
  EXPECT_EQ(attempt_login_future.Get().value(),
            LoginStatusResult::kRequiresButtonClick);

  auto* request = content::webid::FederatedEmbedderLoginRequest::Get(
      delegate_->web_contents());
  ASSERT_TRUE(request);
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kAccountIsSignUp);

  EXPECT_CALL(*mock_cleaning_service, ClearConflictingPermissions).Times(0);
}

TEST_F(ActorLoginDelegateImplTest,
       PrimaryPageChangedDuringPasswordAttemptLogin) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kActorLogin);
  GURL url = GURL(kTestUrl);
  url::Origin origin = url::Origin::Create(url);
  const Credential credential =
      CreateTestCredential(kTestUsername, url, origin);
  const autofill::FormData form_data = CreateSigninFormData(url);

  std::vector<password_manager::PasswordForm> saved_forms;
  saved_forms.push_back(CreateSavedPasswordForm(url, kTestUsername));
  form_fetcher_.SetBestMatches(saved_forms);

  ON_CALL(mock_driver_, GetLastCommittedOrigin())
      .WillByDefault(ReturnRef(origin));
  ON_CALL(mock_driver_, IsInPrimaryMainFrame).WillByDefault(Return(true));
  ON_CALL(mock_driver_, IsNestedWithinFencedFrame).WillByDefault(Return(false));

  form_managers_.clear();
  form_managers_.push_back(
      CreateFormManagerWithParsedForm(origin, form_data, mock_driver_));

  SetUpActorCredentialFillerDeps();
  EXPECT_CALL(mock_form_cache_, GetFormManagers)
      .WillRepeatedly(Return(base::span(form_managers_)));

  base::test::TestFuture<LoginStatusResultOrError> future;
  delegate_->AttemptLogin(credential, false, mqls_logger(),
                          base::TimeTicks::Now(), future.GetCallback(),
                          /*action_sequence_delegate=*/nullptr);

  // Trigger `PrimaryPageChanged` before the message loop runs.
  delegate_->PrimaryPageChanged(delegate_->web_contents()->GetPrimaryPage());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value(),
            LoginStatusResult::kErrorPageChangedDuringFilling);
}

}  // namespace actor_login
