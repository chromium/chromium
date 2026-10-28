// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"

#include <memory>
#include <string>
#include <utility>
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
#include "base/test/with_feature_override.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/device_reauth/mock_device_auth_availability_checker.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/password_specifics.pb.h"
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

using testing::_;
using testing::AllOf;
using testing::ByMove;
using testing::Field;
using testing::IsEmpty;
using testing::Not;
using testing::Property;
using testing::Return;

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
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthAvailabilityChecker>,
              GetDeviceAuthAvailabilityChecker,
              (),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthAvailabilityChecker*),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequiredLegacy,
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
    mock_client_ = MockChromePasswordManagerClient::CreateForWebContentsAndGet(
        web_contents());
    ON_CALL(*mock_client_, IsReauthBeforeFillingRequired)
        .WillByDefault(Return(false));
    ON_CALL(*mock_client_, IsReauthBeforeFillingRequiredLegacy)
        .WillByDefault(Return(false));
  }

  void TearDown() override {
    last_dialog_controller_ = nullptr;
    if (profile_store_) {
      profile_store_->ShutdownOnUIThread();
    }
    mock_client_ = nullptr;
    mock_sync_service_ = nullptr;
    mock_sharing_service_ = nullptr;
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

  bool RunSharingFlowAndSelect(
      mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>&
          remote) {
    PasswordForm form;
    form.signon_realm = "https://google.com/";
    form.url = GURL("https://google.com");
    form.username_value = u"user";
    form.password_value = PasswordString(u"pass");
    form.in_store = PasswordForm::Store::kProfileStore;
    profile_store_->AddLogin(FromPasswordForm(form));

    content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
    base::test::TestFuture<bool> result;
    base::test::TestFuture<void> dialog_shown_future;
    dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
    remote->RequestAgentAuthentication(account_info_.GetGaiaId().ToString(),
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

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<syncer::MockSyncService> mock_sync_service_ = nullptr;
  scoped_refptr<TestPasswordStore> profile_store_;
  raw_ptr<MockRemoteActorCredentialSharingService> mock_sharing_service_ =
      nullptr;

  AccountInfo account_info_;

  // Dialog tracking
  std::vector<std::unique_ptr<PasswordForm>> last_dialog_credentials_;
  std::string last_dialog_credential_domain_;
  base::OnceCallback<void(std::optional<PasswordForm>)> last_dialog_callback_;
  base::OnceClosure dialog_shown_quit_closure_;
  raw_ptr<StubRemoteActorSelectionDialogController> last_dialog_controller_ =
      nullptr;

  raw_ptr<MockChromePasswordManagerClient> mock_client_ = nullptr;

 private:
  policy::ScopedManagementServiceOverrideForTesting
      platform_management_service_override_{
          policy::ManagementServiceFactory::GetForPlatform(),
          policy::EnterpriseManagementAuthority::NONE};
  base::test::ScopedFeatureList feature_list_{
      features::kRemoteActorCredentialSharing};
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
                                     /*task_id=*/"actor_id",
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
                                     /*task_id=*/"actor_id",
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
                                     /*task_id=*/long_string,
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
                                     /*task_id=*/"actor\nhack",
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
                                     /*task_id=*/"actor_id", base::DoNothing());
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
                                     /*task_id=*/"actor_id", base::DoNothing());
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
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.GetGaiaId().ToString(),
      /*domain=*/"google.com", /*task_id=*/"actor_id", result.GetCallback());

  // Wait for the dialog to be shown (factory called).
  dialog_shown_future.Get();

  ASSERT_EQ(last_dialog_credentials_.size(), 1u);
  EXPECT_EQ(last_dialog_credentials_[0]->username_value, u"user");
  EXPECT_EQ(last_dialog_credential_domain_, "https://google.com");

  EXPECT_CALL(
      *mock_sharing_service_,
      SharePassword(
          AllOf(Field(&RemoteActorCredentialSharingService::ShareParameters::
                          password_data,
                      Property(&sync_pb::PasswordSpecificsData::username_value,
                               "user")),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          password_data,
                      Property(&sync_pb::PasswordSpecificsData::password_value,
                               "pass")),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          web_origin,
                      "https://google.com"),
                Field(&RemoteActorCredentialSharingService::ShareParameters::
                          task_id,
                      "actor_id"),
                Property(&RemoteActorCredentialSharingService::ShareParameters::
                             password_client_tag_hash,
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
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.GetGaiaId().ToString(),
      /*domain=*/"google.com", /*task_id=*/"actor_id", result.GetCallback());

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
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.GetGaiaId().ToString(),
      /*domain=*/"google.com", /*task_id=*/"actor_id", result.GetCallback());

  dialog_shown_future.Get();

  ASSERT_EQ(last_dialog_credentials_.size(), 1u);

  // Simulate user cancelling the dialog.
  SimulateDialogSelection(std::nullopt);

  EXPECT_FALSE(result.Get());

  histograms.ExpectUniqueSample(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kUserCancelledDialog, 1);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
class RemoteActorCredentialSharingImplReauthTest
    : public base::test::WithFeatureOverride,
      public RemoteActorCredentialSharingImplTest {
 public:
  RemoteActorCredentialSharingImplReauthTest()
      : base::test::WithFeatureOverride(
            password_manager::features::kDeviceReauthRefactor) {}

  bool is_refactor_enabled() const { return IsParamFeatureEnabled(); }
};

TEST_P(RemoteActorCredentialSharingImplReauthTest, ReauthEnabled_Success) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  device_reauth::MockDeviceAuthenticator* raw_authenticator =
      authenticator.get();

  std::u16string expected_message = l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_FILLING_REAUTH, u"google.com");
  EXPECT_CALL(*raw_authenticator, AuthenticateWithMessage(expected_message, _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  EXPECT_CALL(*mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(std::move(authenticator))));

  ON_CALL(*mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  ON_CALL(*mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequired)
      .Times(is_refactor_enabled() ? 1 : 0);
  EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .Times(is_refactor_enabled() ? 0 : 1);

  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_TRUE(RunSharingFlowAndSelect(remote));
}

TEST_P(RemoteActorCredentialSharingImplReauthTest, ReauthEnabled_Failure) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  device_reauth::MockDeviceAuthenticator* raw_authenticator =
      authenticator.get();

  std::u16string expected_message = l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_FILLING_REAUTH, u"google.com");
  EXPECT_CALL(*raw_authenticator, AuthenticateWithMessage(expected_message, _))
      .WillOnce(base::test::RunOnceCallback<1>(false));

  EXPECT_CALL(*mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(std::move(authenticator))));

  ON_CALL(*mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  ON_CALL(*mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequired)
      .Times(is_refactor_enabled() ? 1 : 0);
  EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .Times(is_refactor_enabled() ? 0 : 1);

  EXPECT_FALSE(RunSharingFlowAndSelect(remote));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    RemoteActorCredentialSharingImplReauthTest);

TEST_F(RemoteActorCredentialSharingImplTest, ReauthDisabled_NoPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kDeviceReauthRefactor);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();

  ON_CALL(*mock_client_, IsReauthBeforeFillingRequired)
      .WillByDefault(Return(false));
  EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequired);
  EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequiredLegacy).Times(0);
  EXPECT_CALL(*mock_client_, GetDeviceAuthenticator).Times(0);

  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_TRUE(RunSharingFlowAndSelect(remote));
}

TEST_F(RemoteActorCredentialSharingImplTest,
       ReauthDisabled_NoPrompt_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kDeviceReauthRefactor);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage).Times(0);
  EXPECT_CALL(*mock_client_, GetDeviceAuthenticator)
      .WillOnce(Return(ByMove(std::move(authenticator))));

  ON_CALL(*mock_client_, IsReauthBeforeFillingRequiredLegacy)
      .WillByDefault(Return(false));
  EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequired).Times(0);
  EXPECT_CALL(*mock_client_, IsReauthBeforeFillingRequiredLegacy);

  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_TRUE(RunSharingFlowAndSelect(remote));
}
#endif

TEST_F(RemoteActorCredentialSharingImplTest,
       RejectDuplicateRequestWhenRequestAlreadyInProgress) {
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
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> first_result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.GetGaiaId().ToString(),
      /*domain=*/"google.com", /*task_id=*/"actor_id",
      first_result.GetCallback());

  dialog_shown_future.Get();

  // A second request while the first request is active (dialog shown) should be
  // immediately rejected with false and log kRequestAlreadyInProgress.
  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> second_result;
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.GetGaiaId().ToString(),
      /*domain=*/"google.com", /*task_id=*/"actor_id_2",
      second_result.GetCallback());

  EXPECT_FALSE(second_result.Get());
  histograms.ExpectBucketCount(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kRequestAlreadyInProgress, 1);

  // The first request should still proceed unaffected.
  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  SimulateDialogSelection(*last_dialog_credentials_[0]);
  EXPECT_TRUE(first_result.Get());

  histograms.ExpectBucketCount(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kSuccess, 1);
}

TEST_F(RemoteActorCredentialSharingImplTest,
       WebContentsDestroyedWhileRequestInProgress) {
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
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result_future;
  base::test::TestFuture<void> disconnect_future;
  remote.set_disconnect_handler(disconnect_future.GetCallback());

  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(
      /*gaia_id=*/account_info_.GetGaiaId().ToString(),
      /*domain=*/"google.com", /*task_id=*/"actor_id",
      result_future.GetCallback());

  dialog_shown_future.Get();

  // Destroy the WebContents (and therefore the RenderFrameHost and
  // DocumentUserData) while the authentication request dialog is in flight.
  mock_client_ = nullptr;
  DeleteContents();

  // The remote should observe a disconnection on the pipe.
  EXPECT_TRUE(disconnect_future.Wait());
  // The callback should NOT be run when the tab is closed.
  EXPECT_FALSE(result_future.IsReady());
}

TEST_F(RemoteActorCredentialSharingImplTest, PolicyDisabled_RequestRejected) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();
  SetSyncActive(true);

  // Set the enterprise policy pref to Disabled.
  profile()->GetTestingPrefService()->SetManagedPref(
      glic::prefs::kGlicSparkPolicySettings,
      base::Value(
          std::to_underlying(glic::prefs::GlicSparkPolicyState::kDisabled)));

  base::HistogramTester histograms;
  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(account_info_.GetGaiaId().ToString(),
                                     "google.com", "actor_id",
                                     result.GetCallback());
  EXPECT_FALSE(result.Get());
  histograms.ExpectBucketCount(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kBlockedByPolicy, 1);
}

TEST_F(RemoteActorCredentialSharingImplTest, PolicyEnabled_RequestAllowed) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();
  SetSyncActive(true);

  // Set the enterprise policy pref to Enabled.
  profile()->GetTestingPrefService()->SetManagedPref(
      glic::prefs::kGlicSparkPolicySettings,
      base::Value(
          std::to_underlying(glic::prefs::GlicSparkPolicyState::kEnabled)));

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(account_info_.GetGaiaId().ToString(),
                                     "google.com", "actor_id",
                                     result.GetCallback());
  dialog_shown_future.Get();
  ASSERT_EQ(last_dialog_credentials_.size(), 1u);
  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  SimulateDialogSelection(*last_dialog_credentials_[0]);
  EXPECT_TRUE(result.Get());
}

TEST_F(RemoteActorCredentialSharingImplTest,
       UnmanagedProfile_AllowedByDefault) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();
  SetSyncActive(true);

  // By default, the profile is unmanaged, and kGlicSparkPolicySettings defaults
  // to kDisabled. Unmanaged profiles should still be allowed to proceed.
  ASSERT_EQ(
      profile()->GetPrefs()->GetInteger(glic::prefs::kGlicSparkPolicySettings),
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kDisabled));
  ASSERT_FALSE(profile()->GetPrefs()->IsManagedPreference(
      glic::prefs::kGlicSparkPolicySettings));

  PasswordForm form;
  form.signon_realm = "https://google.com/";
  form.url = GURL("https://google.com");
  form.username_value = u"user";
  form.password_value = PasswordString(u"pass");
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store_->AddLogin(FromPasswordForm(form));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  base::test::TestFuture<void> dialog_shown_future;
  dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
  remote->RequestAgentAuthentication(account_info_.GetGaiaId().ToString(),
                                     "google.com", "actor_id",
                                     result.GetCallback());
  dialog_shown_future.Get();
  ASSERT_EQ(last_dialog_credentials_.size(), 1u);
  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  SimulateDialogSelection(*last_dialog_credentials_[0]);
  EXPECT_TRUE(result.Get());
}

TEST_F(RemoteActorCredentialSharingImplTest,
       ManagedProfileWithoutPolicy_RequestRejected) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();
  SetSyncActive(true);

  // Mark the profile as enterprise-managed, without setting any policy pref.
  // The pref remains unmanaged at its default value (kDisabled), but because
  // the profile is managed, the request must be rejected.
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::CLOUD);

  base::HistogramTester histograms;
  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(account_info_.GetGaiaId().ToString(),
                                     "google.com", "actor_id",
                                     result.GetCallback());
  EXPECT_FALSE(result.Get());
  histograms.ExpectBucketCount(
      "PasswordManager.RemoteActorCredentialSharing.Result",
      RemoteActorCredentialSharingResult::kBlockedByPolicy, 1);
}

}  // namespace password_manager
