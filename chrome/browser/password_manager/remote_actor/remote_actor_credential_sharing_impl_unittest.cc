// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

namespace {

class MockBadMessageHelper {
 public:
  MockBadMessageHelper() {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &MockBadMessageHelper::OnBadMessage, base::Unretained(this)));
  }
  ~MockBadMessageHelper() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  MOCK_METHOD(void, OnBadMessage, (const std::string& error));
};

class StubRemoteActorSelectionDialogController
    : public RemoteActorSelectionDialogController {
 public:
  StubRemoteActorSelectionDialogController(
      content::WebContents* web_contents,
      std::vector<std::unique_ptr<PasswordForm>> credentials,
      const std::string& credential_domain,
      OnResultCallback callback)
      : RemoteActorSelectionDialogController(web_contents,
                                             std::move(credentials),
                                             credential_domain,
                                             std::move(callback)) {}
  ~StubRemoteActorSelectionDialogController() override = default;
  void Show() override {}
};

class MockChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  static MockChromePasswordManagerClient* CreateForWebContentsAndGet(
      content::WebContents* contents) {
    auto* client =
        new testing::NiceMock<MockChromePasswordManagerClient>(contents);
    contents->SetUserData(UserDataKey(), base::WrapUnique(client));
    return client;
  }

  explicit MockChromePasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents) {}

  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthenticator*),
              (override));
};

class MockRemoteActorCredentialSharingService
    : public RemoteActorCredentialSharingService {
 public:
  MockRemoteActorCredentialSharingService() = default;
  ~MockRemoteActorCredentialSharingService() override = default;

  MOCK_METHOD(
      void,
      SharePassword,
      (const RemoteActorCredentialSharingService::ShareParameters& params,
       RemoteActorCredentialSharingService::SharePasswordCallback callback),
      (override));
};

std::unique_ptr<KeyedService> CreateMockRemoteActorCredentialSharingService(
    content::BrowserContext* context) {
  return std::make_unique<
      testing::NiceMock<MockRemoteActorCredentialSharingService>>();
}

}  // namespace
class RemoteActorCredentialSharingImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ~RemoteActorCredentialSharingImplTest() override = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    mock_sync_service_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      testing::NiceMock<syncer::MockSyncService>>();
                })));

    profile_store_ = CreateAndUseTestPasswordStore(profile());
    account_store_ = CreateAndUseTestAccountPasswordStore(profile());

    mock_sharing_service_ =
        static_cast<MockRemoteActorCredentialSharingService*>(
            RemoteActorCredentialSharingServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating(
                        &CreateMockRemoteActorCredentialSharingService)));

    // Default sync config: active.
    SetSyncActive(true);
    autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
    mock_client_ =
        MockChromePasswordManagerClient::CreateForWebContentsAndGet(
            web_contents());
    ON_CALL(*mock_client_, IsReauthBeforeFillingRequired)
        .WillByDefault(testing::Return(false));
    mock_match_helper_ = std::make_unique<
        testing::NiceMock<password_manager::MockAffiliatedMatchHelper>>(
        &fake_affiliation_service_);
    profile_store_->SetAffiliatedMatchHelper(mock_match_helper_.get());
  }

  void TearDown() override {
    last_dialog_controller_ = nullptr;
    if (profile_store_) {
      profile_store_->ShutdownOnUIThread();
    }
    if (account_store_) {
      account_store_->ShutdownOnUIThread();
    }
    mock_client_ = nullptr;
    mock_sync_service_ = nullptr;
    mock_sharing_service_ = nullptr;
    mock_match_helper_.reset();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void NavigateAndCommit(const GURL& url) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);
    if (main_rfh()) {
      main_rfh()->GetProcess()->Init();
    }
  }

  void SignIn(const std::string& email) {
    account_info_ =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable(email, signin::ConsentLevel::kSignin);
  }

  void SetSyncActive(bool active) {
    ON_CALL(*mock_sync_service_, GetDisableReasons())
        .WillByDefault(
            testing::Return(syncer::SyncService::DisableReasonSet()));
    ON_CALL(*mock_sync_service_, HasSyncConsent())
        .WillByDefault(testing::Return(active));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete())
        .WillByDefault(testing::Return(active));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(testing::Return(
            active ? syncer::UserSelectableTypeSet(
                         {syncer::UserSelectableType::kPasswords})
                   : syncer::UserSelectableTypeSet()));
    ON_CALL(*mock_sync_service_, GetActiveDataTypes())
        .WillByDefault(
            testing::Return(active ? syncer::DataTypeSet({syncer::PASSWORDS})
                                   : syncer::DataTypeSet()));
  }

  void SetAccountStorageState(bool active) {
    ON_CALL(*mock_sync_service_, HasSyncConsent())
        .WillByDefault(testing::Return(false));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete())
        .WillByDefault(testing::Return(false));
    ON_CALL(*mock_sync_service_, GetTransportState())
        .WillByDefault(testing::Return(
            active ? syncer::SyncService::TransportState::ACTIVE
                   : syncer::SyncService::TransportState::DISABLED));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(testing::Return(
            active ? syncer::UserSelectableTypeSet(
                         {syncer::UserSelectableType::kPasswords})
                   : syncer::UserSelectableTypeSet()));
    ON_CALL(*mock_sync_service_, GetActiveDataTypes())
        .WillByDefault(
            testing::Return(active ? syncer::DataTypeSet({syncer::PASSWORDS})
                                   : syncer::DataTypeSet()));
  }

  void CreateImpl(content::RenderFrameHost* rfh = nullptr) {
    if (!rfh) {
      rfh = main_rfh();
    }
    RemoteActorCredentialSharingImpl::CreateForCurrentDocument(
        rfh, base::BindRepeating(
                 &RemoteActorCredentialSharingImplTest::CreateTestDialog,
                 base::Unretained(this)));
  }
  std::unique_ptr<RemoteActorSelectionDialogController> CreateTestDialog(
      content::WebContents* web_contents,
      std::vector<std::unique_ptr<PasswordForm>> credentials,
      const std::string& credential_domain,
      base::OnceCallback<void(std::optional<PasswordForm>)> callback) {
    last_dialog_credentials_ = std::move(credentials);
    last_dialog_credential_domain_ = credential_domain;
    last_dialog_callback_ = std::move(callback);

    auto controller =
        std::make_unique<StubRemoteActorSelectionDialogController>(
            web_contents, std::vector<std::unique_ptr<PasswordForm>>(),
            credential_domain,
            base::BindOnce(
                &RemoteActorCredentialSharingImplTest::OnDialogResult,
                base::Unretained(this)));
    last_dialog_controller_ = controller.get();
    if (dialog_shown_quit_closure_) {
      std::move(dialog_shown_quit_closure_).Run();
    }
    return controller;
  }

  void OnDialogResult(std::optional<PasswordForm> form) {
    last_dialog_controller_ = nullptr;
    ASSERT_TRUE(last_dialog_callback_);
    std::move(last_dialog_callback_).Run(form);
  }

  void SimulateDialogSelection(std::optional<PasswordForm> selected_form) {
    ASSERT_TRUE(last_dialog_controller_);
    if (selected_form) {
      last_dialog_controller_->OnChooseCredentials(
          *selected_form,
          password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
    } else {
      last_dialog_controller_->OnCloseDialog();
    }
  }

  void SetupMockAuthenticator(bool reauth_required,
                              bool auth_success = false,
                              const std::string& domain = "google.com") {
    auto prepared_authenticator = std::make_unique<
        testing::NiceMock<device_reauth::MockDeviceAuthenticator>>();
    auto* raw_authenticator = prepared_authenticator.get();

    if (reauth_required) {
      EXPECT_CALL(*raw_authenticator, CanAuthenticateWithBiometrics)
          .WillRepeatedly(testing::Return(true));
      std::u16string expected_message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
      expected_message = l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_FILLING_REAUTH, base::UTF8ToUTF16(domain));
#endif
      EXPECT_CALL(*raw_authenticator,
                  AuthenticateWithMessage(expected_message, testing::_))
          .WillOnce(testing::WithArg<1>(
              [auth_success](device_reauth::DeviceAuthenticator::AuthenticateCallback callback) {
                std::move(callback).Run(auth_success);
              }));
    } else {
      EXPECT_CALL(*raw_authenticator, AuthenticateWithMessage).Times(0);
    }

    EXPECT_CALL(*mock_client_, GetDeviceAuthenticator)
        .WillOnce([authenticator = std::move(prepared_authenticator)]() mutable {
          return std::move(authenticator);
        });
    EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequired)
        .WillOnce(testing::Return(reauth_required));
  }

  bool RunSharingFlowAndSelect(
      mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>& remote) {
    PasswordForm form;
    form.signon_realm = "https://google.com/";
    form.url = GURL("https://google.com");
    form.username_value = u"user";
    form.password_value = u"pass";
    form.in_store = PasswordForm::Store::kProfileStore;
    profile_store_->AddLogin(FromPasswordForm(form));

    content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
    base::test::TestFuture<bool> result;
    base::test::TestFuture<void> dialog_shown_future;
    dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
    remote->RequestAgentAuthentication(account_info_.gaia.ToString(),
                                       "google.com", "actor_id",
                                       result.GetCallback());
    dialog_shown_future.Get();

    if (last_dialog_credentials_.size() != 1u) {
      return false;
    }

    SimulateDialogSelection(*last_dialog_credentials_[0]);
    return result.Get();
  }

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>
  SetUpAndBindFlow() {
    SignIn("user@gmail.com");
    NavigateAndCommit(GURL("https://gemini.google.com"));
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    CreateImpl();

    RemoteActorCredentialSharingImpl* impl =
        RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
    EXPECT_NE(impl, nullptr);

    mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
    if (impl) {
      impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());
    }
    return remote;
  }

  void SetAffiliatedAndGroupedRealms(
      const password_manager::PasswordFormDigest& observed_form,
      const std::vector<std::string>& affiliated_realms,
      const std::vector<std::string>& grouped_realms = {}) {
    mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
        observed_form, affiliated_realms, grouped_realms);
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<syncer::MockSyncService> mock_sync_service_ = nullptr;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;
  raw_ptr<MockRemoteActorCredentialSharingService> mock_sharing_service_ =
      nullptr;

  AccountInfo account_info_;

  // Dialog tracking
  std::vector<std::unique_ptr<PasswordForm>> last_dialog_credentials_;
  std::string last_dialog_credential_domain_;
  base::OnceCallback<void(std::optional<PasswordForm>)> last_dialog_callback_;
  base::OnceClosure dialog_shown_quit_closure_;
  raw_ptr<StubRemoteActorSelectionDialogController> last_dialog_controller_ = nullptr;

  raw_ptr<MockChromePasswordManagerClient> mock_client_ = nullptr;
  affiliations::FakeAffiliationService fake_affiliation_service_;
  std::unique_ptr<
      testing::NiceMock<password_manager::MockAffiliatedMatchHelper>>
      mock_match_helper_;

 private:
  base::test::ScopedFeatureList feature_list_{
      ::features::kRemoteActorCredentialSharing};
};

// Verify that when the feature flag kRemoteActorCredentialSharing is enabled,
// binding the receiver succeeds on the whitelisted origins, and authentication
// request works.
TEST_F(RemoteActorCredentialSharingImplTest,
       FeatureEnabledWhitelistedOriginSucceeds) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  // Call RequestAgentAuthentication (expected false because not signed in).
  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(/*gaia_id=*/"123456789",
                                     /*domain=*/"google.com",
                                     /*remote_actor_id=*/"actor_id",
                                     result.GetCallback());
  EXPECT_FALSE(result.Get());
}

// Verify that when the feature flag kRemoteActorCredentialSharing is enabled,
// binding the receiver succeeds on the sandbox whitelisted origin.
TEST_F(RemoteActorCredentialSharingImplTest,
       FeatureEnabledSandboxOriginSucceeds) {
  NavigateAndCommit(GURL("https://gemini-preprod.corp.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  // Call RequestAgentAuthentication (expected false).
  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(/*gaia_id=*/"123456789",
                                     /*domain=*/"google.com",
                                     /*remote_actor_id=*/"actor_id",
                                     result.GetCallback());
  EXPECT_FALSE(result.Get());
}

// Verify behavior with empty Gaia ID (returns false).
TEST_F(RemoteActorCredentialSharingImplTest,
       RequestWithEmptyGaiaIdReturnsFalse) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(/*gaia_id=*/"", /*domain=*/"google.com",
                                     /*remote_actor_id=*/"actor_id",
                                     result.GetCallback());
  EXPECT_FALSE(result.Get());
}

// Verify behavior with extremely long strings.
TEST_F(RemoteActorCredentialSharingImplTest,
       RequestWithExtremelyLongArguments) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  std::string long_string(1000000, 'a');

  MockBadMessageHelper bad_message_helper;
  base::test::TestFuture<std::string> bad_message_future;
  EXPECT_CALL(bad_message_helper, OnBadMessage)
      .WillOnce([&bad_message_future](const std::string& error) {
        bad_message_future.SetValue(error);
      });

  remote->RequestAgentAuthentication(/*gaia_id=*/long_string,
                                     /*domain=*/"google.com",
                                     /*remote_actor_id=*/long_string,
                                     base::DoNothing());
  EXPECT_THAT(
      bad_message_future.Get(),
      testing::HasSubstr(
          "RemoteActorCredentialSharing: Argument length limit exceeded"));
}

// Verify behavior with special/null characters.
TEST_F(RemoteActorCredentialSharingImplTest, RequestWithSpecialCharacters) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(/*gaia_id=*/"gaia\0id",
                                     /*domain=*/"google.com",
                                     /*remote_actor_id=*/"actor\nhack",
                                     result.GetCallback());
  EXPECT_FALSE(result.Get());
}

// Verify that calling RequestAgentAuthentication from a subframe
// triggers a bad message.
TEST_F(RemoteActorCredentialSharingImplTest,
       SubframeRequestTriggersBadMessage) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();

  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://gemini.google.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  ASSERT_NE(subframe, nullptr);
  subframe->GetProcess()->Init();

  CreateImpl(subframe);
  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(subframe);
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  MockBadMessageHelper bad_message_helper;
  base::test::TestFuture<std::string> bad_message_future;
  EXPECT_CALL(bad_message_helper, OnBadMessage)
      .WillOnce([&bad_message_future](const std::string& error) {
        bad_message_future.SetValue(error);
      });

  remote->RequestAgentAuthentication(/*gaia_id=*/"123456789",
                                     /*domain=*/"google.com",
                                     /*remote_actor_id=*/"actor_id",
                                     base::DoNothing());
  EXPECT_THAT(bad_message_future.Get(),
              testing::HasSubstr(
                  "RemoteActorCredentialSharing: Request from subframe"));
}

// Verify that calling RequestAgentAuthentication without user activation
// reports a bad message.
TEST_F(RemoteActorCredentialSharingImplTest,
       RequestWithoutUserGestureReportsBadMessage) {
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  MockBadMessageHelper bad_message_helper;
  base::test::TestFuture<std::string> bad_message_future;
  EXPECT_CALL(bad_message_helper, OnBadMessage)
      .WillOnce([&bad_message_future](const std::string& error) {
        bad_message_future.SetValue(error);
      });

  remote->RequestAgentAuthentication(/*gaia_id=*/"123456789",
                                     /*domain=*/"google.com",
                                     /*remote_actor_id=*/"actor_id",
                                     base::DoNothing());
  EXPECT_THAT(
      bad_message_future.Get(),
      testing::HasSubstr(
          "RemoteActorCredentialSharing: Request without user gesture"));
}

TEST_F(RemoteActorCredentialSharingImplTest, SuccessFlow_SelectCredential) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  // Add a credential to the profile store (synced because sync is active).
  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = u"pass";
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.gaia.ToString(),
      /*domain=*/"google.com", /*remote_actor_id=*/"actor_id",
      result.GetCallback());

  // Wait for the dialog to be shown (factory called).
  dialog_shown_future.Get();

  ASSERT_EQ(last_dialog_credentials_.size(), 1u);
  EXPECT_EQ(last_dialog_credentials_[0]->username_value, u"user");
  EXPECT_EQ(last_dialog_credential_domain_, "https://google.com");

  using ::testing::_;
  using ::testing::AllOf;
  using ::testing::Field;
  using ::testing::IsEmpty;
  using ::testing::Not;

  EXPECT_CALL(
      *mock_sharing_service_,
      SharePassword(AllOf(Field(&RemoteActorCredentialSharingService::
                                    ShareParameters::password_data,
                                ::testing::Property(
                                    &sync_pb::PasswordSpecificsData::
                                        username_value,
                                    "user")),
                          Field(&RemoteActorCredentialSharingService::
                                    ShareParameters::password_data,
                                ::testing::Property(
                                    &sync_pb::PasswordSpecificsData::
                                        password_value,
                                    "pass")),
                          Field(&RemoteActorCredentialSharingService::
                                    ShareParameters::web_origin,
                                "https://google.com"),
                          Field(&RemoteActorCredentialSharingService::
                                    ShareParameters::agent_oauth_client_id,
                                "actor_id"),
                          Field(&RemoteActorCredentialSharingService::
                                    ShareParameters::password_client_tag_hash,
                                Not(IsEmpty()))),
                    _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  // Simulate user selecting the credential.
  SimulateDialogSelection(*last_dialog_credentials_[0]);

  // Selecting a credential should return true (success).
  EXPECT_TRUE(result.Get());

  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSuccess, 1);
}

TEST_F(RemoteActorCredentialSharingImplTest, FailureFlow_SharingFailed) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = u"pass";
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.gaia.ToString(),
      /*domain=*/"google.com", /*remote_actor_id=*/"actor_id",
      result.GetCallback());

  dialog_shown_future.Get();

  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(false));

  // Simulate user selecting the credential.
  SimulateDialogSelection(*last_dialog_credentials_[0]);

  // Selecting a credential should return false (failure).
  EXPECT_FALSE(result.Get());

  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSharingFailed, 1);
}

TEST_F(RemoteActorCredentialSharingImplTest, SuccessFlow_CancelDialog) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());
  ASSERT_NE(impl, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = u"pass";
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.gaia.ToString(),
      /*domain=*/"google.com", /*remote_actor_id=*/"actor_id",
      result.GetCallback());

  dialog_shown_future.Get();

  ASSERT_EQ(last_dialog_credentials_.size(), 1u);

  // Simulate user cancelling the dialog.
  SimulateDialogSelection(std::nullopt);

  EXPECT_FALSE(result.Get());

  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kUserCancelledDialog, 1);
}

TEST_F(RemoteActorCredentialSharingImplTest,
       SyncInactive_OnlyAccountStoreQueried) {
  SignIn("user@gmail.com");
  SetAccountStorageState(true);
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  // Add password to profile store (local-only because sync is inactive).
  PasswordForm profile_form;
  profile_form.signon_realm = "https://google.com/";
  profile_form.url = GURL("https://google.com");
  profile_form.username_value = u"profile_user";
  profile_form.password_value = u"pass";
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(profile_form));

  // Add password to account store.
  PasswordForm account_form;
  account_form.signon_realm = "https://google.com/";
  account_form.url = GURL("https://google.com");
  account_form.username_value = u"account_user";
  account_form.password_value = u"pass";
  account_form.in_store = PasswordForm::Store::kAccountStore;
  account_store_->AddLogin(FromPasswordForm(account_form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.gaia.ToString(),
      /*domain=*/"google.com", /*remote_actor_id=*/"actor_id",
      result.GetCallback());

  dialog_shown_future.Get();

  // Only account_user should be in the dialog.
  ASSERT_EQ(last_dialog_credentials_.size(), 1u);
  EXPECT_EQ(last_dialog_credentials_[0]->username_value, u"account_user");

  SimulateDialogSelection(std::nullopt);
}

TEST_F(RemoteActorCredentialSharingImplTest,
       SyncActive_OnlyProfileStoreQueried) {
  SignIn("user@gmail.com");
  SetSyncActive(true);
  NavigateAndCommit(GURL("https://gemini.google.com"));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  CreateImpl();

  RemoteActorCredentialSharingImpl* impl =
      RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh());

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());

  // Add password to profile store (will be kept because sync is active).
  PasswordForm profile_form;
  profile_form.signon_realm = "https://google.com/";
  profile_form.url = GURL("https://google.com");
  profile_form.username_value = u"profile_user";
  profile_form.password_value = u"pass";
  profile_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(profile_form));

  // Add password to account store (should be ignored because sync feature is
  // active, which means account store is not active).
  PasswordForm account_form;
  account_form.signon_realm = "https://google.com/";
  account_form.url = GURL("https://google.com");
  account_form.username_value = u"account_user";
  account_form.password_value = u"pass";
  account_form.in_store = PasswordForm::Store::kAccountStore;
  account_store_->AddLogin(FromPasswordForm(account_form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.gaia.ToString(),
      /*domain=*/"google.com", /*remote_actor_id=*/"actor_id",
      result.GetCallback());

  dialog_shown_future.Get();

  // Only profile_user should be in the dialog.
  ASSERT_EQ(last_dialog_credentials_.size(), 1u);
  EXPECT_EQ(last_dialog_credentials_[0]->username_value, u"profile_user");

  SimulateDialogSelection(std::nullopt);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_F(RemoteActorCredentialSharingImplTest, ReauthEnabled_Success) {
  auto remote = SetUpAndBindFlow();
  SetupMockAuthenticator(/*reauth_required=*/true, /*auth_success=*/true);
  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_TRUE(RunSharingFlowAndSelect(remote));
}

TEST_F(RemoteActorCredentialSharingImplTest, ReauthEnabled_Failure) {
  auto remote = SetUpAndBindFlow();
  SetupMockAuthenticator(/*reauth_required=*/true, /*auth_success=*/false);
  EXPECT_FALSE(RunSharingFlowAndSelect(remote));
}

TEST_F(RemoteActorCredentialSharingImplTest, ReauthDisabled_NoPrompt) {
  auto remote = SetUpAndBindFlow();
  SetupMockAuthenticator(/*reauth_required=*/false);
  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_TRUE(RunSharingFlowAndSelect(remote));
}
#endif

TEST_F(RemoteActorCredentialSharingImplTest,
       RequestFailsWhenTrustedVaultKeyRequired) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();

  // Set trusted vault key required.
  ON_CALL(*mock_sync_service_->GetMockUserSettings(),
          IsTrustedVaultKeyRequiredForPreferredDataTypes())
      .WillByDefault(testing::Return(true));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(account_info_.gaia.ToString(),
                                     "google.com", "actor_id",
                                     result.GetCallback());
  EXPECT_FALSE(result.Get());
}

TEST_F(RemoteActorCredentialSharingImplTest,
       ExactAndStrongAffiliationsAllowed_PSLAndWeakIgnored) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();

  PasswordForm exact_form;
  exact_form.signon_realm = "https://example.com/";
  exact_form.url = GURL("https://example.com");
  exact_form.username_value = u"exact_user";
  exact_form.password_value = u"pass";
  exact_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(exact_form));

  PasswordForm affiliated_form;
  affiliated_form.signon_realm = "https://affiliated.com/";
  affiliated_form.url = GURL("https://affiliated.com");
  affiliated_form.username_value = u"affiliated_user";
  affiliated_form.password_value = u"pass";
  affiliated_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(affiliated_form));

  PasswordForm psl_form;
  psl_form.signon_realm = "https://m.example.com/";
  psl_form.url = GURL("https://m.example.com");
  psl_form.username_value = u"psl_user";
  psl_form.password_value = u"pass";
  psl_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(psl_form));

  PasswordForm grouped_form;
  grouped_form.signon_realm = "https://grouped.com/";
  grouped_form.url = GURL("https://grouped.com");
  grouped_form.username_value = u"grouped_user";
  grouped_form.password_value = u"pass";
  grouped_form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(grouped_form));

  SetAffiliatedAndGroupedRealms(
      PasswordFormDigest(PasswordForm::Scheme::kHtml, "https://example.com/",
                         GURL("https://example.com/")),
      /*affiliated_realms=*/{"https://affiliated.com/"},
      /*grouped_realms=*/{"https://grouped.com/"});

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.gaia.ToString(),
      /*domain=*/"example.com", /*remote_actor_id=*/"actor_id",
      result.GetCallback());

  dialog_shown_future.Get();

  // Only exact_user and affiliated_user should be included in the dialog;
  // psl_user and grouped_user must be ignored.
  ASSERT_EQ(last_dialog_credentials_.size(), 2u);
  EXPECT_EQ(last_dialog_credentials_[0]->username_value, u"exact_user");
  EXPECT_EQ(last_dialog_credentials_[1]->username_value, u"affiliated_user");

  SimulateDialogSelection(std::nullopt);
}

}  // namespace password_manager
