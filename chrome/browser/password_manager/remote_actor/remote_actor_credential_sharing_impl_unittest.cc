// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_service.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using testing::_;
using testing::Return;

constexpr int kAIModeAllowed = 0;
constexpr int kAIModeDisabled = 1;
constexpr char kResultHistogram[] =
    "PasswordManager.RemoteActorCredentialSharing.Result";
constexpr char kGeminiResultHistogram[] =
    "PasswordManager.RemoteActorCredentialSharing.Gemini.Result";
constexpr char kSearchResultHistogram[] =
    "PasswordManager.RemoteActorCredentialSharing.Search.Result";
constexpr char kGeminiOrigin[] = "https://gemini.google.com";
constexpr char kSearchOrigin[] = "https://search.google.com";

class MockBadMessageHelper {
 public:
  MockBadMessageHelper() = default;
  ~MockBadMessageHelper() = default;

  MOCK_METHOD(void, OnBadMessage, (const std::string& error));

 private:
  mojo::ScopedDefaultProcessErrorHandlerForTesting error_handler_{
      base::BindRepeating(&MockBadMessageHelper::OnBadMessage,
                          base::Unretained(this))};
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

class RemoteActorCredentialSharingImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  RemoteActorCredentialSharingImplTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kRemoteActorCredentialSharing},
        /*disabled_features=*/{features::kRemoteActorCredentialSharingSearch});
  }
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
    ChromePasswordManagerClient::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    last_dialog_controller_ = nullptr;
    opener_web_contents_.reset();
    if (profile_store_) {
      profile_store_->ShutdownOnUIThread();
    }
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
        .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
    ON_CALL(*mock_sync_service_, HasSyncConsent())
        .WillByDefault(Return(active));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete())
        .WillByDefault(Return(active));
    ON_CALL(*mock_sync_service_->GetMockUserSettings(), GetSelectedTypes())
        .WillByDefault(
            Return(active ? syncer::UserSelectableTypeSet(
                                {syncer::UserSelectableType::kPasswords})
                          : syncer::UserSelectableTypeSet()));
    ON_CALL(*mock_sync_service_, GetActiveDataTypes())
        .WillByDefault(Return(active ? syncer::DataTypeSet({syncer::PASSWORDS})
                                     : syncer::DataTypeSet()));
  }

  std::unique_ptr<RemoteActorSelectionDialogController> CreateTestDialog(
      content::WebContents* web_contents,
      std::vector<std::unique_ptr<PasswordForm>> credentials,
      const std::string& credential_domain,
      base::OnceCallback<void(std::optional<PasswordForm>)> callback) {
    last_dialog_credentials_ = std::move(credentials);
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

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>
  BindForFrame(content::RenderFrameHost* rfh) {
    RemoteActorCredentialSharingImpl::CreateForCurrentDocument(
        rfh, base::BindRepeating(
                 &RemoteActorCredentialSharingImplTest::CreateTestDialog,
                 base::Unretained(this)));
    RemoteActorCredentialSharingImpl* impl =
        RemoteActorCredentialSharingImpl::GetForCurrentDocument(rfh);
    EXPECT_NE(impl, nullptr);

    mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
    if (impl) {
      impl->Bind(remote.BindNewEndpointAndPassDedicatedReceiver());
    }
    return remote;
  }

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>
  SetUpAndBindFlow(const GURL& origin = GURL(kGeminiOrigin),
                   bool sign_in = true) {
    if (sign_in) {
      SignIn("user@gmail.com");
    }
    NavigateAndCommit(origin);
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    return BindForFrame(main_rfh());
  }

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>
  SetUpPopupAndBind(const GURL& opener_url,
                    ::ui::mojom::WindowOpenDisposition disposition,
                    bool has_user_gesture) {
    opener_web_contents_ = CreateTestWebContents();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        opener_web_contents_.get(), opener_url);
    content::WebContentsTester::For(web_contents())
        ->SetOpener(opener_web_contents_.get());
    content::WebContentsTester::For(web_contents())
        ->SetOriginalWindowOpenDisposition(disposition);

    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateRendererInitiated(
            GURL(kGeminiOrigin), main_rfh());
    navigation->SetHasUserGesture(has_user_gesture);
    navigation->Commit();
    main_rfh()->GetProcess()->Init();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    EXPECT_FALSE(main_rfh()->HasTransientUserActivation());
    EXPECT_EQ(main_rfh()->IsLastCrossDocumentNavigationStartedByUser(),
              has_user_gesture);
    return BindForFrame(main_rfh());
  }

  void AddProfileLogin() {
    PasswordForm form;
    form.url = GURL("https://google.com");
    form.signon_realm = form.url.spec();
    form.username_value = u"user";
    form.password_value = PasswordString(u"pass");
    form.in_store = PasswordForm::Store::kProfileStore;
    profile_store_->AddLogin(FromPasswordForm(form));
  }

  bool RequestAuth(mojo::AssociatedRemote<
                       chrome::mojom::RemoteActorCredentialSharing>& remote,
                   const std::string& gaia_id = "123456789",
                   const std::string& domain = "google.com",
                   const std::string& task_id = "actor_id",
                   bool simulate_user_activation = true) {
    if (simulate_user_activation) {
      content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
    }
    base::test::TestFuture<bool> result;
    remote->RequestAgentAuthentication(gaia_id, domain, task_id,
                                       result.GetCallback());
    return result.Get();
  }

  base::test::TestFuture<bool> StartAuthAndWaitForDialog(
      mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>&
          remote,
      const std::string& task_id = "actor_id") {
    content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
    base::test::TestFuture<bool> result;
    base::test::TestFuture<void> dialog_shown_future;
    dialog_shown_quit_closure_ = dialog_shown_future.GetCallback();
    remote->RequestAgentAuthentication(account_info_.GetGaiaId().ToString(),
                                       "google.com", task_id,
                                       result.GetCallback());
    dialog_shown_future.Get();
    return result;
  }

  void AddTestLoginAndVerifyRequestAllowed(
      mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>&
          remote) {
    AddProfileLogin();
    base::test::TestFuture<bool> result = StartAuthAndWaitForDialog(remote);
    ASSERT_EQ(last_dialog_credentials_.size(), 1u);
    EXPECT_CALL(*mock_sharing_service_, SharePassword)
        .WillOnce(base::test::RunOnceCallback<1>(true));
    SimulateDialogSelection(*last_dialog_credentials_[0]);
    EXPECT_TRUE(result.Get());
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<syncer::MockSyncService> mock_sync_service_ = nullptr;
  scoped_refptr<TestPasswordStore> profile_store_;
  raw_ptr<MockRemoteActorCredentialSharingService> mock_sharing_service_ =
      nullptr;
  std::unique_ptr<content::WebContents> opener_web_contents_;

  AccountInfo account_info_;

  // Dialog tracking.
  std::vector<std::unique_ptr<PasswordForm>> last_dialog_credentials_;
  base::OnceCallback<void(std::optional<PasswordForm>)> last_dialog_callback_;
  base::OnceClosure dialog_shown_quit_closure_;
  raw_ptr<StubRemoteActorSelectionDialogController> last_dialog_controller_ =
      nullptr;

 private:
  policy::ScopedManagementServiceOverrideForTesting
      platform_management_service_override_{
          policy::ManagementServiceFactory::GetForPlatform(),
          policy::EnterpriseManagementAuthority::NONE};
  base::test::ScopedFeatureList feature_list_;
};

// Verify that when `kRemoteActorCredentialSharing` is enabled, binding succeeds
// on allowlisted production and sandbox origins and reaches identity checks.
TEST_F(RemoteActorCredentialSharingImplTest,
       FeatureEnabledAllowlistedOriginsSucceed) {
  for (const char* origin :
       {kGeminiOrigin, "https://gemini-preprod.corp.google.com"}) {
    SCOPED_TRACE(origin);
    base::HistogramTester histograms;
    NavigateAndCommit(GURL(origin));
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
    RemoteActorCredentialSharingImpl::BindReceiver(
        remote.BindNewEndpointAndPassDedicatedReceiver(), main_rfh());
    ASSERT_NE(
        RemoteActorCredentialSharingImpl::GetForCurrentDocument(main_rfh()),
        nullptr);
    // Expected false because the user is not signed in.
    EXPECT_FALSE(RequestAuth(remote));
    histograms.ExpectUniqueSample(
        kResultHistogram,
        RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid, 1);
    histograms.ExpectUniqueSample(
        kGeminiResultHistogram,
        RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid, 1);
  }
}

// Verify that arguments at the exact 255-character upper bound pass
// precondition checks without reporting a Mojo bad message.
TEST_F(RemoteActorCredentialSharingImplTest, AcceptsMaxLengthArguments) {
  base::HistogramTester histograms;
  MockBadMessageHelper bad_message_helper;
  EXPECT_CALL(bad_message_helper, OnBadMessage).Times(0);
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow(GURL(kGeminiOrigin), /*sign_in=*/false);

  // Exact upper boundary (255 characters) must pass precondition checks
  // without reporting a bad message (fails later because user is not signed
  // in).
  const std::string max_valid_arg(255, 'a');
  EXPECT_FALSE(
      RequestAuth(remote, max_valid_arg, max_valid_arg, max_valid_arg));
  histograms.ExpectUniqueSample(
      kResultHistogram,
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid, 1);
  histograms.ExpectUniqueSample(
      kGeminiResultHistogram,
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid, 1);
}

// Verify that 256+ characters in any argument triggers a Mojo bad message.
TEST_F(RemoteActorCredentialSharingImplTest, ArgumentLengthLimitEnforcement) {
  struct OverLimitCase {
    const char* name;
    std::string gaia_id;
    std::string domain;
    std::string task_id;
  };
  const std::string over_limit_arg(256, 'a');
  const std::vector<OverLimitCase> cases = {
      {"gaia_id", over_limit_arg, "google.com", "actor_id"},
      {"domain", "123456789", over_limit_arg, "actor_id"},
      {"task_id", "123456789", "google.com", over_limit_arg},
  };

  for (const OverLimitCase& test_case : cases) {
    SCOPED_TRACE(test_case.name);
    mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing>
        case_remote = SetUpAndBindFlow(GURL(kGeminiOrigin), /*sign_in=*/false);
    content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();

    MockBadMessageHelper bad_message_helper;
    base::test::TestFuture<std::string> bad_message_future;
    EXPECT_CALL(bad_message_helper, OnBadMessage)
        .WillOnce([&bad_message_future](const std::string& error) {
          bad_message_future.SetValue(error);
        });

    case_remote->RequestAgentAuthentication(test_case.gaia_id, test_case.domain,
                                            test_case.task_id,
                                            base::DoNothing());
    EXPECT_THAT(
        bad_message_future.Get(),
        testing::HasSubstr(
            "RemoteActorCredentialSharing: Argument length limit exceeded"));
  }
}

// Verify behavior with special/null characters.
TEST_F(RemoteActorCredentialSharingImplTest, RequestWithSpecialCharacters) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow(GURL(kGeminiOrigin), /*sign_in=*/false);
  EXPECT_FALSE(RequestAuth(remote, std::string("gaia\0id", 7), "google.com",
                           "actor\nhack"));
}

// Verify that `BindReceiver()` ignores subframes and does not attach an impl.
TEST_F(RemoteActorCredentialSharingImplTest, BindReceiverIgnoresSubframe) {
  NavigateAndCommit(GURL(kGeminiOrigin));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kGeminiOrigin), content::RenderFrameHostTester::For(main_rfh())
                                   ->AppendChild("subframe"));
  ASSERT_NE(subframe, nullptr);

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote;
  RemoteActorCredentialSharingImpl::BindReceiver(
      remote.BindNewEndpointAndPassDedicatedReceiver(), subframe);
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());
  EXPECT_EQ(RemoteActorCredentialSharingImpl::GetForCurrentDocument(subframe),
            nullptr);
}

// Verify that calling `RequestAgentAuthentication()` from a subframe triggers a
// bad message.
TEST_F(RemoteActorCredentialSharingImplTest,
       SubframeRequestTriggersBadMessage) {
  NavigateAndCommit(GURL(kGeminiOrigin));
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();

  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kGeminiOrigin), content::RenderFrameHostTester::For(main_rfh())
                                   ->AppendChild("subframe"));
  ASSERT_NE(subframe, nullptr);
  subframe->GetProcess()->Init();

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      BindForFrame(subframe);

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

// Verify that calling `RequestAgentAuthentication()` without user activation
// fails precondition validation and returns false.
TEST_F(RemoteActorCredentialSharingImplTest,
       RequestWithoutUserGestureReturnsFalse) {
  base::HistogramTester histograms;
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow(GURL(kGeminiOrigin), /*sign_in=*/false);

  EXPECT_FALSE(RequestAuth(remote, "123456789", "google.com", "actor_id",
                           /*simulate_user_activation=*/false));
  histograms.ExpectTotalCount(kResultHistogram, 0);
}

// Verify that a popup with a same-site opener navigated with a user gesture can
// make a single `RequestAgentAuthentication()` call without in-popup transient
// user activation, and that a subsequent call without transient activation is
// rejected.
TEST_F(RemoteActorCredentialSharingImplTest,
       PopupWithUserInitiatedNavigationAllowsOneRequestWithoutGesture) {
  base::HistogramTester histograms;
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpPopupAndBind(GURL("https://accounts.google.com"),
                        ::ui::mojom::WindowOpenDisposition::kNewPopup,
                        /*has_user_gesture=*/true);

  // First request in the popup without transient activation passes precondition
  // validation (and returns false because not signed in).
  EXPECT_FALSE(RequestAuth(remote, "123456789", "google.com", "actor_id",
                           /*simulate_user_activation=*/false));
  histograms.ExpectUniqueSample(
      kResultHistogram,
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid, 1);

  // Second request in the same document without transient activation fails
  // precondition validation because the one-time popup activation was consumed.
  EXPECT_FALSE(RequestAuth(remote, "123456789", "google.com", "actor_id",
                           /*simulate_user_activation=*/false));
  histograms.ExpectTotalCount(kResultHistogram, 1);
}

struct PopupRejectionTestCase {
  std::string test_name;
  std::string opener_url;
  ::ui::mojom::WindowOpenDisposition disposition;
  bool has_user_gesture = true;
};

class RemoteActorCredentialSharingPopupRejectionTest
    : public RemoteActorCredentialSharingImplTest,
      public testing::WithParamInterface<PopupRejectionTestCase> {};

TEST_P(RemoteActorCredentialSharingPopupRejectionTest,
       RejectsRequestWithoutTransientActivation) {
  const PopupRejectionTestCase& test_case = GetParam();
  base::HistogramTester histograms;
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpPopupAndBind(GURL(test_case.opener_url), test_case.disposition,
                        test_case.has_user_gesture);

  EXPECT_FALSE(RequestAuth(remote, "123456789", "google.com", "actor_id",
                           /*simulate_user_activation=*/false));
  histograms.ExpectTotalCount(kResultHistogram, 0);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    RemoteActorCredentialSharingPopupRejectionTest,
    testing::Values(
        PopupRejectionTestCase{
            .test_name = "PopupWithCrossSiteOpener",
            .opener_url = "https://example.com",
            .disposition = ::ui::mojom::WindowOpenDisposition::kNewPopup},
        PopupRejectionTestCase{
            .test_name = "NonPopupWithSameSiteOpener",
            .opener_url = "https://accounts.google.com",
            .disposition =
                ::ui::mojom::WindowOpenDisposition::kNewForegroundTab},
        PopupRejectionTestCase{
            .test_name = "PopupWithoutUserInitiatedNavigation",
            .opener_url = "https://accounts.google.com",
            .disposition = ::ui::mojom::WindowOpenDisposition::kNewPopup,
            .has_user_gesture = false}),
    [](const testing::TestParamInfo<PopupRejectionTestCase>& info) {
      return info.param.test_name;
    });

TEST_F(RemoteActorCredentialSharingImplTest,
       RejectDuplicateRequestWhenRequestAlreadyInProgress) {
  base::HistogramTester histograms;
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();
  AddProfileLogin();

  base::test::TestFuture<bool> first_result = StartAuthAndWaitForDialog(remote);

  // A second request while the first request is active (dialog shown) should be
  // immediately rejected with false and log `kRequestAlreadyInProgress`.
  EXPECT_FALSE(RequestAuth(remote, account_info_.GetGaiaId().ToString(),
                           "google.com", "actor_id_2"));
  histograms.ExpectBucketCount(
      kResultHistogram,
      RemoteActorCredentialSharingResult::kRequestAlreadyInProgress, 1);
  histograms.ExpectBucketCount(
      kGeminiResultHistogram,
      RemoteActorCredentialSharingResult::kRequestAlreadyInProgress, 1);

  // The first request should still proceed unaffected.
  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(base::test::RunOnceCallback<1>(true));
  SimulateDialogSelection(*last_dialog_credentials_[0]);
  EXPECT_TRUE(first_result.Get());

  histograms.ExpectBucketCount(kResultHistogram,
                               RemoteActorCredentialSharingResult::kSuccess, 1);
  histograms.ExpectBucketCount(kGeminiResultHistogram,
                               RemoteActorCredentialSharingResult::kSuccess, 1);
}

TEST_F(RemoteActorCredentialSharingImplTest,
       DialogCancellationLogsResultAndAllowsSubsequentRequest) {
  base::HistogramTester histograms;
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();
  AddProfileLogin();

  using ShareParams = RemoteActorCredentialSharingService::ShareParameters;
  EXPECT_CALL(
      *mock_sharing_service_,
      SharePassword(testing::Field(&ShareParams::task_id, "actor_id_2"), _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  // First request is cancelled by the user in the dialog.
  base::test::TestFuture<bool> first_result = StartAuthAndWaitForDialog(remote);
  SimulateDialogSelection(std::nullopt);
  EXPECT_FALSE(first_result.Get());

  // After completion, a subsequent request in the same document must succeed.
  base::test::TestFuture<bool> second_result =
      StartAuthAndWaitForDialog(remote, "actor_id_2");
  SimulateDialogSelection(*last_dialog_credentials_[0]);
  EXPECT_TRUE(second_result.Get());

  EXPECT_THAT(
      histograms.GetAllSamples(kResultHistogram),
      base::BucketsAre(
          base::Bucket(RemoteActorCredentialSharingResult::kUserCancelledDialog,
                       1),
          base::Bucket(RemoteActorCredentialSharingResult::kSuccess, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples(kGeminiResultHistogram),
      base::BucketsAre(
          base::Bucket(RemoteActorCredentialSharingResult::kUserCancelledDialog,
                       1),
          base::Bucket(RemoteActorCredentialSharingResult::kSuccess, 1)));
}

TEST_F(RemoteActorCredentialSharingImplTest,
       OffTheRecordProfile_RequestRejected) {
  base::HistogramTester histograms;
  SignIn("user@gmail.com");
  TestingProfile* otr_profile =
      TestingProfile::Builder().BuildIncognito(profile());
  std::unique_ptr<content::WebContents> otr_contents =
      content::WebContentsTester::CreateTestWebContents(otr_profile, nullptr);
  autofill::ChromeAutofillClient::CreateForWebContents(otr_contents.get());
  ChromePasswordManagerClient::CreateForWebContents(otr_contents.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      otr_contents.get(), GURL(kGeminiOrigin));
  content::RenderFrameHost* otr_rfh = otr_contents->GetPrimaryMainFrame();
  otr_rfh->GetProcess()->Init();
  content::RenderFrameHostTester::For(otr_rfh)->InitializeRenderFrameIfNeeded();

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      BindForFrame(otr_rfh);
  content::RenderFrameHostTester::For(otr_rfh)->SimulateUserActivation();

  base::test::TestFuture<bool> result;
  remote->RequestAgentAuthentication(account_info_.GetGaiaId().ToString(),
                                     "google.com", "actor_id",
                                     result.GetCallback());
  EXPECT_FALSE(result.Get());
  histograms.ExpectUniqueSample(
      kResultHistogram,
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid, 1);
  histograms.ExpectUniqueSample(
      kGeminiResultHistogram,
      RemoteActorCredentialSharingResult::kUserIdentityOrSyncStateInvalid, 1);
}

TEST_F(RemoteActorCredentialSharingImplTest,
       WebContentsDestroyedWhileRequestInProgress) {
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow();
  AddProfileLogin();

  base::test::TestFuture<void> disconnect_future;
  remote.set_disconnect_handler(disconnect_future.GetCallback());

  base::test::TestFuture<bool> result_future =
      StartAuthAndWaitForDialog(remote);

  // Destroy the `WebContents` (and therefore the `RenderFrameHost` and
  // `DocumentUserData`) while the authentication request dialog is in flight.
  DeleteContents();

  // The remote should observe a disconnection on the pipe, and the callback
  // should not be run when the tab is closed.
  EXPECT_TRUE(disconnect_future.Wait());
  EXPECT_FALSE(result_future.IsReady());
}

struct PolicyMatrixTestCase {
  std::string test_name;
  std::string origin;
  bool enable_search_feature = false;
  bool is_managed_profile = false;
  std::optional<glic::prefs::GlicSparkPolicyState> spark_policy;
  std::optional<int> aim_policy;
  bool expected_allowed = false;
  std::string actor_histogram;
};

class RemoteActorCredentialSharingPolicyMatrixTest
    : public RemoteActorCredentialSharingImplTest,
      public testing::WithParamInterface<PolicyMatrixTestCase> {};

TEST_P(RemoteActorCredentialSharingPolicyMatrixTest, EnforcesEnterprisePolicy) {
  const PolicyMatrixTestCase& test_case = GetParam();
  base::test::ScopedFeatureList search_feature_list;
  if (test_case.enable_search_feature) {
    search_feature_list.InitAndEnableFeature(
        features::kRemoteActorCredentialSharingSearch);
  }

  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow(GURL(test_case.origin));

  std::optional<policy::ScopedManagementServiceOverrideForTesting>
      profile_management_override;
  if (test_case.is_managed_profile) {
    profile_management_override.emplace(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::CLOUD);
  }
  if (test_case.spark_policy.has_value()) {
    profile()->GetTestingPrefService()->SetManagedPref(
        glic::prefs::kGlicSparkPolicySettings,
        base::Value(std::to_underlying(*test_case.spark_policy)));
  }
  if (test_case.aim_policy.has_value()) {
    profile()->GetTestingPrefService()->SetManagedPref(
        omnibox::kAIModeSettings, base::Value(*test_case.aim_policy));
  }

  base::HistogramTester histograms;
  if (test_case.expected_allowed) {
    AddTestLoginAndVerifyRequestAllowed(remote);
    histograms.ExpectUniqueSample(
        kResultHistogram, RemoteActorCredentialSharingResult::kSuccess, 1);
    histograms.ExpectUniqueSample(test_case.actor_histogram,
                                  RemoteActorCredentialSharingResult::kSuccess,
                                  1);
    return;
  }

  EXPECT_FALSE(RequestAuth(remote, account_info_.GetGaiaId().ToString()));
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kBlockedByPolicy,
      1);
  histograms.ExpectUniqueSample(
      test_case.actor_histogram,
      RemoteActorCredentialSharingResult::kBlockedByPolicy, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    RemoteActorCredentialSharingPolicyMatrixTest,
    testing::Values(
        PolicyMatrixTestCase{
            .test_name = "GeminiPolicyDisabled_Rejected",
            .origin = kGeminiOrigin,
            .spark_policy = glic::prefs::GlicSparkPolicyState::kDisabled,
            .actor_histogram = kGeminiResultHistogram},
        PolicyMatrixTestCase{
            .test_name = "GeminiPolicyEnabled_Allowed",
            .origin = kGeminiOrigin,
            .spark_policy = glic::prefs::GlicSparkPolicyState::kEnabled,
            .expected_allowed = true,
            .actor_histogram = kGeminiResultHistogram},
        PolicyMatrixTestCase{
            .test_name = "GeminiUnmanagedProfile_AllowedByDefault",
            .origin = kGeminiOrigin,
            .expected_allowed = true,
            .actor_histogram = kGeminiResultHistogram},
        PolicyMatrixTestCase{
            .test_name = "GeminiManagedProfileWithoutPolicy_Rejected",
            .origin = kGeminiOrigin,
            .is_managed_profile = true,
            .actor_histogram = kGeminiResultHistogram},
        PolicyMatrixTestCase{
            .test_name = "GeminiPolicyDisabledWithAimAllowed_Rejected",
            .origin = kGeminiOrigin,
            .spark_policy = glic::prefs::GlicSparkPolicyState::kDisabled,
            .aim_policy = kAIModeAllowed,
            .actor_histogram = kGeminiResultHistogram},
        PolicyMatrixTestCase{
            .test_name = "GeminiPolicyEnabledWithAimDisabled_Allowed",
            .origin = kGeminiOrigin,
            .spark_policy = glic::prefs::GlicSparkPolicyState::kEnabled,
            .aim_policy = kAIModeDisabled,
            .expected_allowed = true,
            .actor_histogram = kGeminiResultHistogram},
        PolicyMatrixTestCase{
            .test_name = "SearchPolicyDisabledWithSparkEnabled_Rejected",
            .origin = kSearchOrigin,
            .enable_search_feature = true,
            .spark_policy = glic::prefs::GlicSparkPolicyState::kEnabled,
            .aim_policy = kAIModeDisabled,
            .actor_histogram = kSearchResultHistogram},
        PolicyMatrixTestCase{
            .test_name =
                "SearchManagedProfileWithSparkDisabled_AllowedWhenAimAllowed",
            .origin = kSearchOrigin,
            .enable_search_feature = true,
            .is_managed_profile = true,
            .spark_policy = glic::prefs::GlicSparkPolicyState::kDisabled,
            .expected_allowed = true,
            .actor_histogram = kSearchResultHistogram}),
    [](const testing::TestParamInfo<PolicyMatrixTestCase>& info) {
      return info.param.test_name;
    });

struct ActorBrandingTestCase {
  std::string test_name;
  std::string origin;
  bool enable_search_feature = false;
  std::string search_config_json;
  RemoteActorType expected_actor_type;
  std::u16string expected_title;
  std::u16string expected_subtitle;
  std::string expected_learn_more_url;
  std::string expected_icon_url;
  std::string expected_oauth_client_id;
  int expected_task_namespace_id;
  std::string expected_actor_histogram;
  std::string unexpected_actor_histogram;
};

class RemoteActorCredentialSharingActorBrandingTest
    : public RemoteActorCredentialSharingImplTest,
      public testing::WithParamInterface<ActorBrandingTestCase> {};

TEST_P(RemoteActorCredentialSharingActorBrandingTest,
       PlumbsActorBrandingAndAgentParameters) {
  const ActorBrandingTestCase& test_case = GetParam();
  base::test::ScopedFeatureList search_feature_list;
  if (test_case.enable_search_feature) {
    if (test_case.search_config_json.empty()) {
      search_feature_list.InitAndEnableFeature(
          features::kRemoteActorCredentialSharingSearch);
    } else {
      search_feature_list.InitAndEnableFeatureWithParameters(
          features::kRemoteActorCredentialSharingSearch,
          {{"search_config_json", test_case.search_config_json}});
    }
  }

  base::HistogramTester histograms;
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow(GURL(test_case.origin));
  AddProfileLogin();

  base::test::TestFuture<bool> result =
      StartAuthAndWaitForDialog(remote, "actor_task_id");

  ASSERT_NE(last_dialog_controller_, nullptr);
  EXPECT_EQ(last_dialog_controller_->GetActorType(),
            test_case.expected_actor_type);
  EXPECT_EQ(last_dialog_controller_->GetTitle(), test_case.expected_title);
  EXPECT_EQ(last_dialog_controller_->GetSubtitle(),
            test_case.expected_subtitle);
  EXPECT_EQ(last_dialog_controller_->GetLearnMoreUrl(),
            GURL(test_case.expected_learn_more_url));
  EXPECT_EQ(last_dialog_controller_->GetActorIconUrl(),
            GURL(test_case.expected_icon_url));

  RemoteActorCredentialSharingService::ShareParameters captured_params;
  EXPECT_CALL(*mock_sharing_service_, SharePassword)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&captured_params),
                               base::test::RunOnceCallback<1>(true)));
  SimulateDialogSelection(*last_dialog_credentials_[0]);
  EXPECT_TRUE(result.Get());

  EXPECT_EQ(captured_params.actor_oauth_client_id,
            test_case.expected_oauth_client_id);
  EXPECT_EQ(captured_params.task_namespace_id,
            test_case.expected_task_namespace_id);
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kSuccess, 1);
  histograms.ExpectUniqueSample(test_case.expected_actor_histogram,
                                RemoteActorCredentialSharingResult::kSuccess,
                                1);
  histograms.ExpectTotalCount(test_case.unexpected_actor_histogram, 0);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    RemoteActorCredentialSharingActorBrandingTest,
    testing::Values(
        ActorBrandingTestCase{
            .test_name = "GeminiDefault",
            .origin = kGeminiOrigin,
            .expected_actor_type = RemoteActorType::kGemini,
            .expected_title = u"Allow Gemini to sign in to google.com for you?",
            .expected_subtitle =
                u"Gemini can use Google Password Manager to sign in for you. "
                u"Learn how Gemini handles your data",
            .expected_learn_more_url =
                "https://support.google.com/gemini?p=lm_gpm_data",
            .expected_oauth_client_id =
                "320695880279-gnq6the97ga85scn208u5jctnk82qelk."
                "apps.googleusercontent.com",
            .expected_task_namespace_id = 30,
            .expected_actor_histogram = kGeminiResultHistogram,
            .unexpected_actor_histogram = kSearchResultHistogram},
        ActorBrandingTestCase{
            .test_name = "SearchDefault",
            .origin = kSearchOrigin,
            .enable_search_feature = true,
            .expected_actor_type = RemoteActorType::kSearch,
            .expected_title =
                u"Allow Search Agent to sign in to google.com for you?",
            .expected_subtitle =
                u"Search Agent can use Google Password Manager to sign in for "
                u"you. Learn how Search Agent handles your data",
            .expected_learn_more_url = "https://g.co/privacypolicy",
            .expected_oauth_client_id =
                "240392999166-p989rucds3hg2lidtif5huititvl4ln1."
                "apps.googleusercontent.com",
            .expected_task_namespace_id = 41,
            .expected_actor_histogram = kSearchResultHistogram,
            .unexpected_actor_histogram = kGeminiResultHistogram},
        ActorBrandingTestCase{
            .test_name = "SearchWithFinchJsonOverrides",
            .origin = kSearchOrigin,
            .enable_search_feature = true,
            .search_config_json = R"({
              "actor_name": "Custom Search",
              "learn_more_url": "https://example.com/help",
              "icon_url": "https://example.com/icon.png",
              "oauth_client_id": "custom-search-client-id",
              "task_namespace_id": 77
            })",
            .expected_actor_type = RemoteActorType::kSearch,
            .expected_title =
                u"Allow Custom Search to sign in to google.com for you?",
            .expected_subtitle =
                u"Custom Search can use Google Password Manager to sign in for "
                u"you. Learn how Custom Search handles your data",
            .expected_learn_more_url = "https://example.com/help",
            .expected_icon_url = "https://example.com/icon.png",
            .expected_oauth_client_id = "custom-search-client-id",
            .expected_task_namespace_id = 77,
            .expected_actor_histogram = kSearchResultHistogram,
            .unexpected_actor_histogram = kGeminiResultHistogram}),
    [](const testing::TestParamInfo<ActorBrandingTestCase>& info) {
      return info.param.test_name;
    });

struct OriginRejectionTestCase {
  std::string test_name;
  std::string origin;
  bool enable_gemini_feature = false;
  bool enable_search_feature = false;
};

class RemoteActorCredentialSharingOriginRejectionTest
    : public RemoteActorCredentialSharingImplTest,
      public testing::WithParamInterface<OriginRejectionTestCase> {};

TEST_P(RemoteActorCredentialSharingOriginRejectionTest,
       LogsOtherErrorAndReturnsFalse) {
  const OriginRejectionTestCase& test_case = GetParam();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates({
      {features::kRemoteActorCredentialSharing,
       test_case.enable_gemini_feature},
      {features::kRemoteActorCredentialSharingSearch,
       test_case.enable_search_feature},
  });

  base::HistogramTester histograms;
  mojo::AssociatedRemote<chrome::mojom::RemoteActorCredentialSharing> remote =
      SetUpAndBindFlow(GURL(test_case.origin));
  EXPECT_FALSE(RequestAuth(remote, account_info_.GetGaiaId().ToString()));
  histograms.ExpectUniqueSample(
      kResultHistogram, RemoteActorCredentialSharingResult::kOtherError, 1);
  histograms.ExpectTotalCount(kGeminiResultHistogram, 0);
  histograms.ExpectTotalCount(kSearchResultHistogram, 0);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    RemoteActorCredentialSharingOriginRejectionTest,
    testing::Values(
        OriginRejectionTestCase{.test_name = "DisallowedOrigin",
                                .origin = "https://example.com",
                                .enable_gemini_feature = true},
        OriginRejectionTestCase{.test_name = "SearchFeatureDisabled",
                                .origin = kSearchOrigin,
                                .enable_gemini_feature = true},
        OriginRejectionTestCase{.test_name = "GeminiFeatureDisabled",
                                .origin = kGeminiOrigin,
                                .enable_search_feature = true}),
    [](const testing::TestParamInfo<OriginRejectionTestCase>& info) {
      return info.param.test_name;
    });

}  // namespace

}  // namespace password_manager
