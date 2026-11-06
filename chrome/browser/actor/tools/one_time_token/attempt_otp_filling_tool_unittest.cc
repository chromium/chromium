// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/tools/one_time_token/actor_login_flow_verifier.h"
#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_metrics.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/core/task_id.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

using ::testing::_;
using ::testing::Return;

class MockActorOneTimeTokenFillingService
    : public autofill::ActorOneTimeTokenFillingService {
 public:
  MockActorOneTimeTokenFillingService() = default;
  ~MockActorOneTimeTokenFillingService() override = default;

  MOCK_METHOD(autofill::FormFillingContextValidationResult,
              ValidateFormFillingContext,
              (content::WebContents*,
               base::span<const autofill::FieldGlobalId>),
              (const, override));
  MOCK_METHOD(
      void,
      OnPasswordFillingStarted,
      (content::WebContents*, const url::Origin&, bool, base::span<const int>),
      (override));
  MOCK_METHOD(void, AbortLoginTracking, (), (override));
  MOCK_METHOD(std::optional<autofill::ActorLoginContext>,
              ConsumeLoginContext,
              (),
              (override));
  MOCK_METHOD(std::optional<url::Origin>,
              GetLoginContextOrigin,
              (),
              (const, override));
  MOCK_METHOD(bool,
              GetLoginContextShouldUseStrongMatching,
              (),
              (const, override));
  MOCK_METHOD(
      void,
      RetrieveOtp,
      (content::WebContents*,
       const url::Origin&,
       const std::vector<autofill::FieldGlobalId>&,
       bool,
       autofill::ActorOneTimeTokenFillingService::PredictedOtpType,
       base::OnceCallback<
           void(base::expected<std::string,
                               one_time_tokens::OneTimeTokenRetrievalError>)>),
      (override));
  MOCK_METHOD(void,
              FillOtp,
              (content::WebContents*,
               const std::vector<autofill::FieldGlobalId>&,
               const std::string&,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(
      void,
      FetchUserDataProcessingConsent,
      (base::OnceCallback<void(
           std::optional<one_time_tokens::UserDataProcessingConsentStates>)>),
      (override));

  base::WeakPtr<ActorOneTimeTokenFillingService> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockActorOneTimeTokenFillingService> weak_factory_{this};
};

class MockActorLoginFlowVerifier : public ActorLoginFlowVerifier {
 public:
  explicit MockActorLoginFlowVerifier(
      affiliations::AffiliationService& affiliation_service)
      : ActorLoginFlowVerifier(affiliation_service) {}
  ~MockActorLoginFlowVerifier() override = default;

  MOCK_METHOD(void,
              VerifyIsActorLoginFlow,
              (content::FrameTreeNodeId otp_frame_id,
               const url::Origin& otp_frame_origin,
               const url::Origin& main_frame_origin,
               std::optional<url::Origin> context_origin,
               bool should_use_strong_matching,
               base::OnceCallback<std::optional<autofill::ActorLoginContext>()>
                   consume_context_callback,
               base::OnceCallback<void(Result)> callback),
              (override));
};

class FakeToolDelegate : public ToolDelegate {
 public:
  explicit FakeToolDelegate(Profile* profile) : profile_(profile) {}
  ~FakeToolDelegate() override = default;

  Profile& GetProfile() override { return *profile_; }

  AggregatedJournal& GetJournal() override { return journal_; }

  autofill::ActorOneTimeTokenFillingService&
  GetActorOneTimeTokenFillingService() override {
    return mock_otp_service_;
  }

  MOCK_METHOD(void,
              RequestToShowGmailOtpOptInDialog,
              (GmailOtpOptInCallback),
              (override));

  MOCK_METHOD(void,
              RequestToShowGmailOtpConfirmationDialog,
              (const std::string&, GmailOtpConfirmationCallback),
              (override));

  MockActorOneTimeTokenFillingService& mock_otp_service() {
    return mock_otp_service_;
  }

  // Unused overrides:
  actor_login::ActorLoginService& GetActorLoginService() override {
    NOTREACHED();
  }
  autofill::ActorFormFillingService& GetActorFormFillingService() override {
    NOTREACHED();
  }
  favicon::FaviconService* GetFaviconService() override { return nullptr; }
  const EnterprisePolicyChecker& GetEnterprisePolicyChecker() const override {
    NOTREACHED();
  }
  void IsAcceptableNavigationDestination(
      const GURL& url,
      DecisionCallbackWithReason callback) override {}
  void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      const base::flat_map<std::string, gfx::Image>& icons,
      CredentialSelectedCallback callback) override {}
  void SetUserSelectedCredential(
      const CredentialWithPermission& credential,
      base::OnceClosure affiliations_fetched) override {}
  const std::optional<CredentialWithPermission> GetUserSelectedCredential(
      const url::Origin& request_origin) const override {
    return std::nullopt;
  }
  void RequestToShowAutofillSuggestions(
      std::vector<autofill::ActorFormFillingRequest> requests,
      base::WeakPtr<AutofillSelectionDialogEventHandler> event_handler,
      AutofillSuggestionSelectedCallback callback) override {}
  void InterruptFromTool() override {}
  void InterruptFromTool(bool retain_user_control) override {}
  void UninterruptFromTool() override {}
  void EnqueueFollowupAction(std::unique_ptr<ToolRequest> action) override {}
  void AddActorSurface(
      ActorSurfaceHandle actor_surface_handle,
      bool stop_task_on_detach,
      base::OnceCallback<void(mojom::ActionResultPtr)> callback) override {}
  bool HasActorSurface(ActorSurfaceHandle actor_surface_handle) override {
    return false;
  }
  void RemoveActorSurface(ActorSurfaceHandle actor_surface_handle) override {}
  void FailCurrentTool(mojom::ActionResultCode reason) override {}
  base::WeakPtr<actor_login::ActionSequenceDelegate> GetActionSequenceDelegate()
      override {
    return nullptr;
  }
  void OnActionTargetFrameResolved(
      content::RenderFrameHost& target_frame) override {}

 private:
  raw_ptr<Profile> profile_;
  AggregatedJournal journal_;
  MockActorOneTimeTokenFillingService mock_otp_service_;
};

class AttemptOtpFillingToolTest : public testing::Test {
 public:
  AttemptOtpFillingToolTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AttemptOtpFillingToolTest() override = default;

  void TearDown() override {
    actor_surface_.reset();
    mock_tab_.reset();
    delegate_.reset();
    web_contents_ = nullptr;
    web_contents_factory_.reset();
    profile_.reset();
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    delegate_ = std::make_unique<FakeToolDelegate>(profile_.get());
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    web_contents_ = web_contents_factory_->CreateWebContents(profile_.get());
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents_, GURL("https://example.com"));
    EXPECT_CALL(*mock_tab_, GetContents())
        .WillRepeatedly(Return(web_contents_));
    actor_surface_ = std::make_unique<ActorSurfaceImpl>(
        ActorSurfaceHandle::CreateForTesting(), mock_tab_->GetHandle());
    EXPECT_CALL(delegate_->mock_otp_service(), ValidateFormFillingContext)
        .WillRepeatedly(
            Return(autofill::FormFillingContextValidationResult::kSecure));
    ON_CALL(delegate_->mock_otp_service(), GetLoginContextOrigin())
        .WillByDefault(
            Return(url::Origin::Create(GURL("https://example.com"))));
    ON_CALL(delegate_->mock_otp_service(),
            GetLoginContextShouldUseStrongMatching())
        .WillByDefault(Return(false));
    ON_CALL(delegate_->mock_otp_service(), FetchUserDataProcessingConsent)
        .WillByDefault(
            [](base::OnceCallback<void(
                   std::optional<
                       one_time_tokens::UserDataProcessingConsentStates>)>
                   callback) {
              std::move(callback).Run(
                  one_time_tokens::UserDataProcessingConsentStates{
                      .comms_apps = one_time_tokens::ConsentState::kEnabled,
                      .google_apps = one_time_tokens::ConsentState::kEnabled});
            });
  }

  PrefService* prefs() { return profile_->GetPrefs(); }
  FakeToolDelegate& delegate() { return *delegate_; }
  tabs::MockTabInterface& mock_tab() { return *mock_tab_; }

  autofill::ActorLoginContext CreateValidLoginContext() {
    return autofill::ActorLoginContext(
        web_contents_->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
        /*should_use_strong_matching=*/false,
        {{web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId(), 1}});
  }

  AttemptOtpFillingTool CreateTool(
      std::vector<PageTarget> trigger_fields,
      bool for_signin = true,
      AttemptOtpFillingToolRequest::OtpType predicted_otp_type =
          AttemptOtpFillingToolRequest::OtpType::kUnknown,
      std::unique_ptr<ActorLoginFlowVerifier> verifier = nullptr) {
    if (!verifier) {
      verifier =
          std::make_unique<ActorLoginFlowVerifier>(fake_affiliation_service_);
    }
    return AttemptOtpFillingTool(TaskId(1), delegate(), *actor_surface_,
                                 std::move(trigger_fields), for_signin,
                                 predicted_otp_type, std::move(verifier));
  }

  // Invokes the tool with `FillOtp()` pending, destroys the tool and expects
  // `kCancelledDuringFillOtp` as the only sample.
  void InvokeAndExpectCancelledDuringFillOtp();

  // Ensures `tool` passes time-of-use validation for `target` by setting up an
  // `AnnotatedPageContent` with an element corresponding to `target`.
  void SetupSuccessfulTimeOfUseValidation(AttemptOtpFillingTool& tool,
                                          const PageTarget& target) {
    optimization_guide::proto::AnnotatedPageContent observation;
    if (std::holds_alternative<DomNode>(target)) {
      const auto& dom_node = std::get<DomNode>(target);
      observation.mutable_main_frame_data()
          ->mutable_document_identifier()
          ->set_serialized_token(dom_node.document_identifier);
      observation.mutable_root_node()
          ->mutable_content_attributes()
          ->set_common_ancestor_dom_node_id(dom_node.node_id);
    } else if (std::holds_alternative<gfx::Point>(target)) {
      const auto& point = std::get<gfx::Point>(target);
      std::string doc_token = optimization_guide::DocumentIdentifierUserData::
                                  GetOrCreateForCurrentDocument(
                                      web_contents_->GetPrimaryMainFrame())
                                      ->serialized_token();
      observation.mutable_main_frame_data()
          ->mutable_document_identifier()
          ->set_serialized_token(doc_token);
      auto* root_node = observation.mutable_root_node();
      auto* bbox = root_node->mutable_content_attributes()
                       ->mutable_geometry()
                       ->mutable_visible_bounding_box();
      bbox->set_x(point.x() - 5);
      bbox->set_y(point.y() - 5);
      bbox->set_width(10);
      bbox->set_height(10);
      root_node->mutable_content_attributes()
          ->mutable_interaction_info()
          ->set_document_scoped_z_order(0);
      root_node->mutable_content_attributes()->set_common_ancestor_dom_node_id(
          1234);
    }
    tool.TimeOfUseValidation(&observation);
  }

 protected:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::optional<content::TestWebContentsFactory> web_contents_factory_{
      std::in_place};
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  affiliations::FakeAffiliationService fake_affiliation_service_;
  std::unique_ptr<FakeToolDelegate> delegate_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<ActorSurfaceImpl> actor_surface_;
  base::HistogramTester histogram_tester_;
};

namespace {  // Have commonly used namespaces here to make tests more readable
using autofill::prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp;
using autofill::prefs::IsAutofillGmailOtpFillingEnabled;
using autofill::prefs::SetAutofillGmailOtpFillingActivationDismissalTimestamp;
using autofill::prefs::SetAutofillGmailOtpFillingEnabled;
using base::test::RunOnceCallback;
using base::test::TestFuture;
using mojom::ActionResultPtr;
using mojom::ActionResultCode::kOk;
using mojom::ActionResultCode::kOtpFieldNotFound;
using mojom::ActionResultCode::kOtpFillFailure;
using mojom::ActionResultCode::kOtpGmailConsentRequired;
using mojom::ActionResultCode::kOtpGoogleConsentRequired;
using mojom::ActionResultCode::kOtpNoLastTabObservation;
using mojom::ActionResultCode::kOtpRetrievalError;
using mojom::ActionResultCode::kOtpTargetFrameNotFound;
using mojom::ActionResultCode::kOtpUnableToFill;
using mojom::ActionResultCode::kOtpUserDeclinedFillingConfirmation;
using mojom::ActionResultCode::kOtpUserDeclinedOptingIntoFilling;
using mojom::ActionResultCode::kTabWentAway;
using mojom::ActionResultCode::kToolTimeout;
using optimization_guide::DocumentIdentifierUserData;
using optimization_guide::proto::AnnotatedPageContent;
using webui::mojom::GmailOtpErrorReason;
using webui::mojom::GmailOtpOptInResult;

std::string ValidateFormFillingContextHistogram(std::string_view step) {
  return base::StrCat(
      {kAttemptOtpFillingValidateFormFillingContextHistogram, ".", step});
}

std::string ConsentFetchDurationHistogram(std::string_view outcome) {
  return base::StrCat(
      {kAttemptOtpFillingConsentFetchDurationHistogram, ".", outcome});
}

std::string ToolInvocationHistogram(std::string_view opt_in_state) {
  return base::StrCat({kAttemptOtpFillingToolHistogram, ".", opt_in_state});
}

void ExpectNoCancellationRecorded(const base::HistogramTester& tester) {
  for (AttemptOtpFillingToolEvent event : {
           AttemptOtpFillingToolEvent::kCancelledDuringConsentFetch,
           AttemptOtpFillingToolEvent::kCancelledDuringOptInCard,
           AttemptOtpFillingToolEvent::kCancelledDuringLoginFlowVerification,
           AttemptOtpFillingToolEvent::kCancelledDuringOtpRetrieval,
           AttemptOtpFillingToolEvent::kCancelledDuringConfirmationDialog,
           AttemptOtpFillingToolEvent::kCancelledDuringFillOtp,
       }) {
    tester.ExpectBucketCount(kAttemptOtpFillingToolHistogram, event, 0);
  }
}
}  // namespace

// When the user grants permission to opt in to Gmail OTPs, the tool
// successfully validates, and persists the decision in the prefs, including to
// remove the dismissal timestamp.
TEST_F(AttemptOtpFillingToolTest, Validate_OptInPermissionGranted) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(RunOnceCallback<0>(GmailOtpOptInResult::NewResponse(
          webui::mojom::GmailOtpOptInResponse::New(true))));
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
  EXPECT_TRUE(IsAutofillGmailOtpFillingEnabled(prefs()));
  EXPECT_EQ(base::Time(),
            GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs()));
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpOptInCardInteractionHistogram,
      GmailOtpOptInCardInteraction::kPermissionGranted, 1);
  // `Validate()` succeeded and the attempt continues to `Invoke()`, so no
  // ToolInvocation outcome is recorded yet.
  histogram_tester_.ExpectTotalCount(kAttemptOtpFillingToolHistogram, 0);
}

// When the user denies permission to opt in to Gmail OTPs, the tool
// fails validation, and persists the decision in the dismissal timestamp pref
// without turning on Gmail OTP.
TEST_F(AttemptOtpFillingToolTest, Validate_OptInPermissionDenied) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(RunOnceCallback<0>(GmailOtpOptInResult::NewResponse(
          webui::mojom::GmailOtpOptInResponse::New(false))));
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpUserDeclinedOptingIntoFilling, future.Take()->code);
  EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(prefs()));
  EXPECT_EQ(base::Time::Now(),
            GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs()));
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kOptInPermissionDenied, 1);
  histogram_tester_.ExpectBucketCount(
      ToolInvocationHistogram("NotOptedIn"),
      AttemptOtpFillingToolEvent::kOptInPermissionDenied, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpOptInCardInteractionHistogram,
      GmailOtpOptInCardInteraction::kPermissionDenied, 1);
}

// When the asking the user for opting into using Gmail OTPs fails, the tool
// fails validation, and doesn't modify any preferences.
TEST_F(AttemptOtpFillingToolTest, Validate_OptInPermissionCallbackError) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(RunOnceCallback<0>(GmailOtpOptInResult::NewErrorReason(
          GmailOtpErrorReason::kRequestPromiseNoSubscriber)));
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpUnableToFill, future.Take()->code);
  EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(prefs()));
  EXPECT_EQ(base::Time::Now() - base::Days(90),
            GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs()));
  histogram_tester_.ExpectUniqueSample(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kOptInErrorResponse, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpOptInCardInteractionHistogram,
      GmailOtpOptInCardInteraction::kErrorResponse, 1);
}

// When Gmail OTP is disabled, but we're within the cool off period for asking
// the user for opting into using Gmail OTPs, the tool fails validation and the
// persisted prefs did not change.
TEST_F(AttemptOtpFillingToolTest,
       Validate_GmailOtpFillingDisabledWithinCoolOffPeriod) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp).Times(0);
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(89));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpUnableToFill, future.Take()->code);
  EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(prefs()));
  EXPECT_EQ(base::Time::Now() - base::Days(89),
            GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs()));
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kWithinOptInCoolOffPeriod, 1);
  histogram_tester_.ExpectBucketCount(
      ToolInvocationHistogram("NotOptedIn"),
      AttemptOtpFillingToolEvent::kWithinOptInCoolOffPeriod, 1);
}

// When Gmail OTP is disabled, and we're outside the cool off period for asking
// the user for opting into using Gmail OTPs, the tool asks the user for opting
// into using Gmail OTPs via RequestToShowGmailOtpOptInDialog.
TEST_F(AttemptOtpFillingToolTest,
       Validate_GmailOtpFillingDisabledOutsideCoolOffPeriod) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  // expect call to RequestToShowGmailOtpOptInDialog, see EXPECT_CALL above
  // Nothing is recorded while the opt-in card is pending.
  histogram_tester_.ExpectTotalCount(kAttemptOtpFillingToolHistogram, 0);
  histogram_tester_.ExpectTotalCount(kGmailOtpOptInCardInteractionHistogram, 0);
}

TEST_F(AttemptOtpFillingToolTest,
       Validate_ConsentFetchSuccess_RecordsDuration) {
  autofill::ActorOneTimeTokenFillingService::
      FetchUserDataProcessingConsentCallback consent_callback;
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([&consent_callback](
                    autofill::ActorOneTimeTokenFillingService::
                        FetchUserDataProcessingConsentCallback callback) {
        consent_callback = std::move(callback);
      });
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  tool.Validate(base::DoNothing());

  task_environment_.FastForwardBy(base::Milliseconds(500));
  one_time_tokens::UserDataProcessingConsentStates states;
  states.comms_apps = one_time_tokens::ConsentState::kEnabled;
  states.google_apps = one_time_tokens::ConsentState::kEnabled;
  std::move(consent_callback).Run(states);

  histogram_tester_.ExpectUniqueSample(
      ConsentFetchDurationHistogram("Success"), 500, 1);
  histogram_tester_.ExpectTotalCount(
      ConsentFetchDurationHistogram("Error"), 0);
}

TEST_F(AttemptOtpFillingToolTest, Validate_ConsentFetchError_RecordsDuration) {
  autofill::ActorOneTimeTokenFillingService::
      FetchUserDataProcessingConsentCallback consent_callback;
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([&consent_callback](
                    autofill::ActorOneTimeTokenFillingService::
                        FetchUserDataProcessingConsentCallback callback) {
        consent_callback = std::move(callback);
      });
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  tool.Validate(base::DoNothing());

  task_environment_.FastForwardBy(base::Milliseconds(500));
  std::move(consent_callback).Run(std::nullopt);

  histogram_tester_.ExpectUniqueSample(
      ConsentFetchDurationHistogram("Error"), 500, 1);
  histogram_tester_.ExpectTotalCount(
      ConsentFetchDurationHistogram("Success"), 0);
}

// Destroying the tool while the consent fetch is pending records
// `kCancelledDuringConsentFetch`. A late response records nothing.
TEST_F(AttemptOtpFillingToolTest, Validate_DestroyedDuringConsentFetch) {
  autofill::ActorOneTimeTokenFillingService::
      FetchUserDataProcessingConsentCallback consent_callback;
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([&consent_callback](
                    autofill::ActorOneTimeTokenFillingService::
                        FetchUserDataProcessingConsentCallback callback) {
        consent_callback = std::move(callback);
      });
  {
    AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
    tool.Validate(base::DoNothing());
    ExpectNoCancellationRecorded(histogram_tester_);
  }

  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kCancelledDuringConsentFetch, 1);
  base::HistogramTester::CountsMap counts_before_late_response =
      histogram_tester_.GetTotalCountsForPrefix("OneTimeTokens.");
  std::move(consent_callback).Run(std::nullopt);
  EXPECT_EQ(counts_before_late_response,
            histogram_tester_.GetTotalCountsForPrefix("OneTimeTokens."));
  histogram_tester_.ExpectTotalCount(ConsentFetchDurationHistogram("Success"), 0);
  histogram_tester_.ExpectTotalCount(ConsentFetchDurationHistogram("Error"), 0);
}

// Destroying the tool while the opt-in card is shown records
// `kCancelledDuringOptInCard` and the card's `kCancelled`. A late response
// records nothing.
TEST_F(AttemptOtpFillingToolTest, Validate_DestroyedDuringOptInCard) {
  ToolDelegate::GmailOtpOptInCallback opt_in_callback;
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(
          [&opt_in_callback](ToolDelegate::GmailOtpOptInCallback callback) {
            opt_in_callback = std::move(callback);
          });
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));
  {
    AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
    tool.Validate(base::DoNothing());
    ExpectNoCancellationRecorded(histogram_tester_);
  }

  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kCancelledDuringOptInCard, 1);
  histogram_tester_.ExpectBucketCount(
      ToolInvocationHistogram("NotOptedIn"),
      AttemptOtpFillingToolEvent::kCancelledDuringOptInCard, 1);
  histogram_tester_.ExpectUniqueSample(kGmailOtpOptInCardInteractionHistogram,
                                       GmailOtpOptInCardInteraction::kCancelled,
                                       1);
  base::HistogramTester::CountsMap counts_before_late_response =
      histogram_tester_.GetTotalCountsForPrefix("OneTimeTokens.");
  std::move(opt_in_callback)
      .Run(GmailOtpOptInResult::NewResponse(
          webui::mojom::GmailOtpOptInResponse::New(true)));
  EXPECT_EQ(counts_before_late_response,
            histogram_tester_.GetTotalCountsForPrefix("OneTimeTokens."));
}

// When the consent fetch and the opt-in card reply synchronously, destroying
// the tool afterwards records no cancellation. This guards against setting the
// pending cancellation event after the asynchronous call instead of before it.
TEST_F(AttemptOtpFillingToolTest,
       Validate_SynchronousCallbacks_NoCancellationOnDestruction) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(RunOnceCallback<0>(GmailOtpOptInResult::NewResponse(
          webui::mojom::GmailOtpOptInResponse::New(true))));
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));
  {
    AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
    TestFuture<ActionResultPtr> future;
    tool.Validate(future.GetCallback());
    EXPECT_EQ(kOk, future.Take()->code);
  }

  ExpectNoCancellationRecorded(histogram_tester_);
}

// Like `ToolController`, destroys the tool from inside the `Validate()`
// callback. The pending cancellation event must already be reset by then.
TEST_F(AttemptOtpFillingToolTest,
       Validate_ToolDestroyedInsideCallback_NoCancellation) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(RunOnceCallback<0>(GmailOtpOptInResult::NewResponse(
          webui::mojom::GmailOtpOptInResponse::New(true))));
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));
  auto tool = std::make_unique<AttemptOtpFillingTool>(
      TaskId(1), delegate(), *actor_surface_,
      std::vector<PageTarget>{PageTarget(gfx::Point(10, 10))},
      /*for_signin=*/true, AttemptOtpFillingToolRequest::OtpType::kUnknown,
      std::make_unique<ActorLoginFlowVerifier>(fake_affiliation_service_));

  tool->Validate(base::BindLambdaForTesting([&tool](ActionResultPtr result) {
    EXPECT_EQ(kOk, result->code);
    tool.reset();
  }));

  EXPECT_FALSE(tool);
  ExpectNoCancellationRecorded(histogram_tester_);
}

// Destroying the tool after a completed `Validate()` and `Invoke()` records no
// cancellation.
TEST_F(AttemptOtpFillingToolTest,
       ValidateThenInvoke_HappyPath_NoCancellationOnDestruction) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(true));
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  PageTarget target(gfx::Point(10, 10));
  {
    AttemptOtpFillingTool tool = CreateTool({target});
    TestFuture<ActionResultPtr> validate_future;
    tool.Validate(validate_future.GetCallback());
    ASSERT_EQ(kOk, validate_future.Take()->code);
    SetupSuccessfulTimeOfUseValidation(tool, target);
    TestFuture<ActionResultPtr> invoke_future;
    tool.Invoke(invoke_future.GetCallback());
    ASSERT_EQ(kOk, invoke_future.Take()->code);
  }

  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFillingOtpSuccess, 1);
  histogram_tester_.ExpectBucketCount(
      ToolInvocationHistogram("AlreadyOptedIn"),
      AttemptOtpFillingToolEvent::kFillingOtpSuccess, 1);
  ExpectNoCancellationRecorded(histogram_tester_);
}

// Destroying the tool during login-flow verification records
// `kCancelledDuringLoginFlowVerification`.
TEST_F(AttemptOtpFillingToolTest, Invoke_DestroyedDuringLoginFlowVerification) {
  auto verifier =
      std::make_unique<testing::NiceMock<MockActorLoginFlowVerifier>>(
          fake_affiliation_service_);
  // Never replies, so the tool keeps waiting.
  EXPECT_CALL(*verifier, VerifyIsActorLoginFlow);
  PageTarget target(gfx::Point(10, 10));
  {
    AttemptOtpFillingTool tool = CreateTool(
        {target}, /*for_signin=*/true,
        AttemptOtpFillingToolRequest::OtpType::kUnknown, std::move(verifier));
    SetupSuccessfulTimeOfUseValidation(tool, target);
    tool.Invoke(base::DoNothing());
    ExpectNoCancellationRecorded(histogram_tester_);
  }

  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kCancelledDuringLoginFlowVerification, 1);
  histogram_tester_.ExpectTotalCount(kAttemptOtpFillingToolHistogram, 1);
}

// Destroying the tool during OTP retrieval records
// `kCancelledDuringOtpRetrieval`.
TEST_F(AttemptOtpFillingToolTest, Invoke_DestroyedDuringOtpRetrieval) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  // Never replies, so the tool keeps waiting.
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp);
  PageTarget target(gfx::Point(10, 10));
  {
    AttemptOtpFillingTool tool = CreateTool({target});
    SetupSuccessfulTimeOfUseValidation(tool, target);
    tool.Invoke(base::DoNothing());
    // Login-flow verification replies asynchronously.
    task_environment_.RunUntilIdle();
    ExpectNoCancellationRecorded(histogram_tester_);
  }

  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kCancelledDuringOtpRetrieval, 1);
  histogram_tester_.ExpectTotalCount(kAttemptOtpFillingToolHistogram, 1);
}

// Destroying the tool during the confirmation dialog records
// `kCancelledDuringConfirmationDialog` and the dialog's `kCancelled`.
TEST_F(AttemptOtpFillingToolTest, Invoke_DestroyedDuringConfirmationDialog) {
  EXPECT_CALL(delegate().mock_otp_service(), GetLoginContextOrigin())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  // Never replies, so the tool keeps waiting.
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _));
  PageTarget target(gfx::Point(10, 10));
  {
    AttemptOtpFillingTool tool = CreateTool({target});
    SetupSuccessfulTimeOfUseValidation(tool, target);
    tool.Invoke(base::DoNothing());
    // Login-flow verification replies asynchronously.
    task_environment_.RunUntilIdle();
    ExpectNoCancellationRecorded(histogram_tester_);
  }

  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kCancelledDuringConfirmationDialog, 1);
  histogram_tester_.ExpectTotalCount(kAttemptOtpFillingToolHistogram, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpConfirmationDialogInteractionHistogram,
      GmailOtpConfirmationDialogInteraction::kCancelled, 1);
}

// Shared by the three `Invoke_DestroyedDuringFillOtp_*` tests below, one per
// `FillOtp()` call site: without confirmation, after confirmation, and with a
// mock OTP.
void AttemptOtpFillingToolTest::InvokeAndExpectCancelledDuringFillOtp() {
  // Never replies, so the tool keeps waiting.
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _));
  PageTarget target(gfx::Point(10, 10));
  {
    AttemptOtpFillingTool tool = CreateTool({target});
    SetupSuccessfulTimeOfUseValidation(tool, target);
    tool.Invoke(base::DoNothing());
    // Login-flow verification replies asynchronously.
    task_environment_.RunUntilIdle();
    ExpectNoCancellationRecorded(histogram_tester_);
  }

  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kCancelledDuringFillOtp, 1);
  histogram_tester_.ExpectTotalCount(kAttemptOtpFillingToolHistogram, 1);
}

TEST_F(AttemptOtpFillingToolTest,
       Invoke_DestroyedDuringFillOtp_WithoutConfirmation) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog).Times(0);
  InvokeAndExpectCancelledDuringFillOtp();
}

TEST_F(AttemptOtpFillingToolTest,
       Invoke_DestroyedDuringFillOtp_AfterConfirmation) {
  EXPECT_CALL(delegate().mock_otp_service(), GetLoginContextOrigin())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _))
      .WillOnce(RunOnceCallback<1>(
          webui::mojom::GmailOtpConfirmationResult::NewResponse(
              webui::mojom::GmailOtpConfirmationResponse::New(
                  /*permission_granted=*/true))));
  InvokeAndExpectCancelledDuringFillOtp();
}

TEST_F(AttemptOtpFillingToolTest, Invoke_DestroyedDuringFillOtp_MockOtp) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kAttemptOtpFillingToolMockValueSkipsChecks, "123456");
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp).Times(0);
  InvokeAndExpectCancelledDuringFillOtp();
}

// Time of use validation returns kTabWentAway when the target tab is not
// available any more.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_TabWentAway) {
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  // The surface is destroyed before its backing tab.
  actor_surface_.reset();
  mock_tab_.reset();
  AnnotatedPageContent observation;

  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(kTabWentAway, result->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kTabWentAwayBeforeInvocation, 1);
}

// Time of use validation returns kOtpNoLastTabObservation when a tool
// invocation request is dispatched without a preceding observation. This might
// be a result of the tab not having been observed (yet), a possible GLIC state.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_NoLastObservation) {
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});

  ActionResultPtr result = tool.TimeOfUseValidation(nullptr);

  EXPECT_EQ(kOtpNoLastTabObservation, result->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kNoLastTabObservation, 1);
}

// Time of use validation returns kOtpFieldNotFound when any of the
// trigger fields aren't found.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_FieldNotFound) {
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  AnnotatedPageContent observation;

  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(kOtpFieldNotFound, result->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kTriggerFieldNotFound, 1);
}

// Time of use validation returns kOk when all conditions are met.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_HappyPath) {
  auto* user_data = DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
      web_contents_->GetPrimaryMainFrame());
  std::string doc_token = user_data->serialized_token();
  AttemptOtpFillingTool tool = CreateTool(
      {PageTarget(DomNode{.node_id = 1234, .document_identifier = doc_token})});
  AnnotatedPageContent observation;
  observation.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(doc_token);
  observation.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(1234);

  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(kOk, result->code);
  histogram_tester_.ExpectTotalCount(kAttemptOtpFillingToolHistogram, 0);
}

// Time of use validation returns `kOtpInsecureContext` when form filling
// context is insecure.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_InsecureContext) {
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(
          Return(autofill::FormFillingContextValidationResult::kInsecurePage));
  auto* user_data = DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
      web_contents_->GetPrimaryMainFrame());
  std::string doc_token = user_data->serialized_token();
  AttemptOtpFillingTool tool = CreateTool(
      {PageTarget(DomNode{.node_id = 1234, .document_identifier = doc_token})});
  AnnotatedPageContent observation;
  observation.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(doc_token);
  observation.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(1234);

  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(mojom::ActionResultCode::kOtpInsecureContext, result->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFormFillingStatusInsecureContext, 1);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("TimeOfUse"),
      autofill::FormFillingContextValidationResult::kInsecurePage, 1);
}

// `Invoke()` returns `kOk` when all conditions are met.
TEST_F(AttemptOtpFillingToolTest, Invoke_HappyPath) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(
      delegate().mock_otp_service(),
      RetrieveOtp(
          _, _, _, _,
          autofill::ActorOneTimeTokenFillingService::PredictedOtpType::kEmail,
          _))
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(true));
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool =
      CreateTool({target}, /*for_signin=*/true,
                 AttemptOtpFillingToolRequest::OtpType::kEmail);
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  SetupSuccessfulTimeOfUseValidation(tool, target);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFillingOtpSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OneTimeTokens.Actor.AttemptOtpFilling.PredictedOtpType",
      AttemptOtpFillingToolRequest::OtpType::kEmail, 1);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Actor_AttemptOtpFilling::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Actor_AttemptOtpFilling::kPredictedOtpTypeName,
      static_cast<int64_t>(AttemptOtpFillingToolRequest::OtpType::kEmail));
}

// `Invoke()` bypasses all checks and dialogs when
// `kAttemptOtpFillingToolMockValueSkipsChecks` is set, directly filling the
// mock value.
TEST_F(AttemptOtpFillingToolTest,
       Invoke_MockOtpValueSkipsChecksSwitch_BypassesChecksAndFillsDirectly) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kAttemptOtpFillingToolMockValueSkipsChecks, "123456");

  // ConsumeLoginContext should be called to clean up any active login tracking,
  // while RetrieveOtp and confirmation dialog are bypassed.
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext).Times(1);
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp).Times(0);
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog).Times(0);

  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(true));

  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool =
      CreateTool({target}, /*for_signin=*/true,
                 AttemptOtpFillingToolRequest::OtpType::kEmail);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFillingOtpSuccess, 1);
  histogram_tester_.ExpectTotalCount(ToolInvocationHistogram("AlreadyOptedIn"),
                                     0);
  histogram_tester_.ExpectTotalCount(ToolInvocationHistogram("NewlyOptedIn"),
                                     0);
  histogram_tester_.ExpectTotalCount(ToolInvocationHistogram("NotOptedIn"), 0);
}

// `Invoke()` fails with `kOtpFillFailure` when the result of
// filling the OTP is false.
TEST_F(AttemptOtpFillingToolTest, Invoke_ErrorFilling) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(false));
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOtpFillFailure, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFillingOtpError, 1);
}

struct RetrieveOtpErrorTestCase {
  std::string_view test_name;
  one_time_tokens::OneTimeTokenRetrievalError error;
  mojom::ActionResultCode expected_code;
  std::string_view expected_message;
  AttemptOtpFillingToolEvent expected_event;
};

class AttemptOtpFillingToolRetrieveOtpErrorTest
    : public AttemptOtpFillingToolTest,
      public testing::WithParamInterface<RetrieveOtpErrorTestCase> {};

// `Invoke()` maps an OTP retrieval error to the result returned to the model
// and records a single ToolInvocation sample.
TEST_P(AttemptOtpFillingToolRetrieveOtpErrorTest, Invoke_RetrievalError) {
  const RetrieveOtpErrorTestCase& test_case = GetParam();
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>(base::unexpected(test_case.error)));
  PageTarget target(gfx::Point(10, 10));
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  {
    AttemptOtpFillingTool tool = CreateTool({target});
    SetupSuccessfulTimeOfUseValidation(tool, target);

    TestFuture<ActionResultPtr> future;
    tool.Invoke(future.GetCallback());

    ActionResultPtr action_result = future.Take();
    EXPECT_EQ(test_case.expected_code, action_result->code);
    EXPECT_EQ(test_case.expected_message, action_result->message);
  }
  histogram_tester_.ExpectUniqueSample(kAttemptOtpFillingToolHistogram,
                                       test_case.expected_event, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AttemptOtpFillingToolRetrieveOtpErrorTest,
    testing::Values(
        RetrieveOtpErrorTestCase{
            .test_name = "Unknown",
            .error = one_time_tokens::OneTimeTokenRetrievalError::kUnknown,
            .expected_code = kOtpRetrievalError,
            .expected_message = "An error occurred during OTP retrieval: 0",
            .expected_event = AttemptOtpFillingToolEvent::kOtpRetrievalError},
        RetrieveOtpErrorTestCase{
            .test_name = "SubscriptionExpired",
            .error = one_time_tokens::OneTimeTokenRetrievalError::
                kSubscriptionExpired,
            .expected_code = mojom::ActionResultCode::kOtpRetrievalTimeout,
            .expected_message = "OTP retrieval timed out.",
            .expected_event = AttemptOtpFillingToolEvent::kOtpRetrievalTimeout},
        // A sender-domain mismatch is reported to the model as a timeout.
        RetrieveOtpErrorTestCase{
            .test_name = "GmailOtpSenderDomainMismatch",
            .error = one_time_tokens::OneTimeTokenRetrievalError::
                kGmailOtpSenderDomainMismatch,
            .expected_code = mojom::ActionResultCode::kOtpRetrievalTimeout,
            .expected_message = "OTP retrieval timed out.",
            .expected_event =
                AttemptOtpFillingToolEvent::kGmailOtpSenderDomainMismatch}),
    [](const testing::TestParamInfo<RetrieveOtpErrorTestCase>& info) {
      return std::string(info.param.test_name);
    });

TEST_F(AttemptOtpFillingToolTest,
       Validate_OptInPermissionCallbackNullResponse) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(RunOnceCallback<0>(nullptr));
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpUnableToFill, future.Take()->code);
  histogram_tester_.ExpectUniqueSample(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kOptInNullResponse, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpOptInCardInteractionHistogram,
      GmailOtpOptInCardInteraction::kErrorResponse, 1);
}

TEST_F(AttemptOtpFillingToolTest, Validate_GmailOtpFillingEnabled) {
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .Times(1);
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
}

TEST_F(AttemptOtpFillingToolTest,
       Validate_GmailOtpFillingEnabled_CommsAppsConsentDisabled) {
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([](base::OnceCallback<void(
                       std::optional<
                           one_time_tokens::UserDataProcessingConsentStates>)>
                       callback) {
        std::move(callback).Run(
            one_time_tokens::UserDataProcessingConsentStates{
                .comms_apps = one_time_tokens::ConsentState::kDisabled,
                .google_apps = one_time_tokens::ConsentState::kEnabled});
      });
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpGmailConsentRequired, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGmailSmartFeaturesConsentRequired, 1);
}

TEST_F(AttemptOtpFillingToolTest,
       Validate_GmailOtpFillingEnabled_GoogleAppsConsentDisabled) {
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([](base::OnceCallback<void(
                       std::optional<
                           one_time_tokens::UserDataProcessingConsentStates>)>
                       callback) {
        std::move(callback).Run(
            one_time_tokens::UserDataProcessingConsentStates{
                .comms_apps = one_time_tokens::ConsentState::kEnabled,
                .google_apps = one_time_tokens::ConsentState::kDisabled});
      });
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpGoogleConsentRequired, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGoogleSmartFeaturesConsentRequired, 1);
}

TEST_F(AttemptOtpFillingToolTest,
       Validate_GmailOtpFillingEnabled_ConsentFetchReturnsNullopt) {
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([](base::OnceCallback<void(
                       std::optional<
                           one_time_tokens::UserDataProcessingConsentStates>)>
                       callback) { std::move(callback).Run(std::nullopt); });
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpUnableToFill, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::
          kUnableToRetrieveGmailAndGoogleSmartFeaturesConsent,
      1);
}

TEST_F(AttemptOtpFillingToolTest, Validate_CommsAppsConsentDisabled) {
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([](base::OnceCallback<void(
                       std::optional<
                           one_time_tokens::UserDataProcessingConsentStates>)>
                       callback) {
        std::move(callback).Run(
            one_time_tokens::UserDataProcessingConsentStates{
                .comms_apps = one_time_tokens::ConsentState::kDisabled,
                .google_apps = one_time_tokens::ConsentState::kEnabled});
      });
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpGmailConsentRequired, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGmailSmartFeaturesConsentRequired, 1);
}

TEST_F(AttemptOtpFillingToolTest, Validate_GoogleAppsConsentDisabled) {
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([](base::OnceCallback<void(
                       std::optional<
                           one_time_tokens::UserDataProcessingConsentStates>)>
                       callback) {
        std::move(callback).Run(
            one_time_tokens::UserDataProcessingConsentStates{
                .comms_apps = one_time_tokens::ConsentState::kEnabled,
                .google_apps = one_time_tokens::ConsentState::kDisabled});
      });
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpGoogleConsentRequired, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGoogleSmartFeaturesConsentRequired, 1);
}

TEST_F(AttemptOtpFillingToolTest, Validate_ConsentFetchReturnsNullopt) {
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce([](base::OnceCallback<void(
                       std::optional<
                           one_time_tokens::UserDataProcessingConsentStates>)>
                       callback) { std::move(callback).Run(std::nullopt); });
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOtpUnableToFill, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::
          kUnableToRetrieveGmailAndGoogleSmartFeaturesConsent,
      1);
  histogram_tester_.ExpectTotalCount(
      kGmailAndGoogleSmartFeaturesConsentHistogram, 0);
}

struct GmailAndGoogleSmartFeaturesConsentTestCase {
  std::string_view test_name;
  one_time_tokens::ConsentState gmail;
  one_time_tokens::ConsentState google;
  ActorOtpGmailAndGoogleSmartFeaturesConsent expected;
};

class AttemptOtpFillingToolGmailAndGoogleSmartFeaturesConsentTest
    : public AttemptOtpFillingToolTest,
      public testing::WithParamInterface<
          GmailAndGoogleSmartFeaturesConsentTestCase> {};

// `Validate()` records the combination of both consent states once the consent
// fetch succeeds.
TEST_P(AttemptOtpFillingToolGmailAndGoogleSmartFeaturesConsentTest,
       Validate_RecordsConsentState) {
  const GmailAndGoogleSmartFeaturesConsentTestCase& test_case = GetParam();
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .WillOnce(
          RunOnceCallback<0>(one_time_tokens::UserDataProcessingConsentStates{
              .comms_apps = test_case.gmail, .google_apps = test_case.google}));
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  histogram_tester_.ExpectUniqueSample(
      kGmailAndGoogleSmartFeaturesConsentHistogram, test_case.expected, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AttemptOtpFillingToolGmailAndGoogleSmartFeaturesConsentTest,
    testing::Values(
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "EnabledEnabled",
            .gmail = one_time_tokens::ConsentState::kEnabled,
            .google = one_time_tokens::ConsentState::kEnabled,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailEnabledGoogleEnabled},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "EnabledDisabled",
            .gmail = one_time_tokens::ConsentState::kEnabled,
            .google = one_time_tokens::ConsentState::kDisabled,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailEnabledGoogleDisabled},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "EnabledUnknown",
            .gmail = one_time_tokens::ConsentState::kEnabled,
            .google = one_time_tokens::ConsentState::kUnknown,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailEnabledGoogleUnknown},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "EnabledUndefined",
            .gmail = one_time_tokens::ConsentState::kEnabled,
            .google = one_time_tokens::ConsentState::kUndefined,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailEnabledGoogleUndefined},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "DisabledEnabled",
            .gmail = one_time_tokens::ConsentState::kDisabled,
            .google = one_time_tokens::ConsentState::kEnabled,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailDisabledGoogleEnabled},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "DisabledDisabled",
            .gmail = one_time_tokens::ConsentState::kDisabled,
            .google = one_time_tokens::ConsentState::kDisabled,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailDisabledGoogleDisabled},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "DisabledUnknown",
            .gmail = one_time_tokens::ConsentState::kDisabled,
            .google = one_time_tokens::ConsentState::kUnknown,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailDisabledGoogleUnknown},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "DisabledUndefined",
            .gmail = one_time_tokens::ConsentState::kDisabled,
            .google = one_time_tokens::ConsentState::kUndefined,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailDisabledGoogleUndefined},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "UnknownEnabled",
            .gmail = one_time_tokens::ConsentState::kUnknown,
            .google = one_time_tokens::ConsentState::kEnabled,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailUnknownGoogleEnabled},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "UnknownDisabled",
            .gmail = one_time_tokens::ConsentState::kUnknown,
            .google = one_time_tokens::ConsentState::kDisabled,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailUnknownGoogleDisabled},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "UnknownUnknown",
            .gmail = one_time_tokens::ConsentState::kUnknown,
            .google = one_time_tokens::ConsentState::kUnknown,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailUnknownGoogleUnknown},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "UnknownUndefined",
            .gmail = one_time_tokens::ConsentState::kUnknown,
            .google = one_time_tokens::ConsentState::kUndefined,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailUnknownGoogleUndefined},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "UndefinedEnabled",
            .gmail = one_time_tokens::ConsentState::kUndefined,
            .google = one_time_tokens::ConsentState::kEnabled,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailUndefinedGoogleEnabled},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "UndefinedDisabled",
            .gmail = one_time_tokens::ConsentState::kUndefined,
            .google = one_time_tokens::ConsentState::kDisabled,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailUndefinedGoogleDisabled},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "UndefinedUnknown",
            .gmail = one_time_tokens::ConsentState::kUndefined,
            .google = one_time_tokens::ConsentState::kUnknown,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailUndefinedGoogleUnknown},
        GmailAndGoogleSmartFeaturesConsentTestCase{
            .test_name = "UndefinedUndefined",
            .gmail = one_time_tokens::ConsentState::kUndefined,
            .google = one_time_tokens::ConsentState::kUndefined,
            .expected = ActorOtpGmailAndGoogleSmartFeaturesConsent::
                kGmailUndefinedGoogleUndefined}),
    [](const testing::TestParamInfo<GmailAndGoogleSmartFeaturesConsentTestCase>&
           info) { return std::string(info.param.test_name); });

// `Validate()` bypasses consent retrieval and opt-in dialog when
// `kAttemptOtpFillingToolMockValueSkipsChecks` is set.
TEST_F(AttemptOtpFillingToolTest,
       Validate_MockOtpValueSkipsChecksSwitch_BypassesConsentAndOptIn) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kAttemptOtpFillingToolMockValueSkipsChecks, "123456");

  // Consent fetching and opt-in dialog should never be called.
  EXPECT_CALL(delegate().mock_otp_service(), FetchUserDataProcessingConsent)
      .Times(0);
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);

  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), false);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
}

TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_FormNotFound) {
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(Return(autofill::FormFillingContextValidationResult::
                           kFormStructureNotFound));
  auto* user_data = DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
      web_contents_->GetPrimaryMainFrame());
  std::string doc_token = user_data->serialized_token();
  AttemptOtpFillingTool tool = CreateTool(
      {PageTarget(DomNode{.node_id = 1234, .document_identifier = doc_token})});
  AnnotatedPageContent observation;
  observation.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(doc_token);
  observation.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(1234);

  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(mojom::ActionResultCode::kFormFillingFieldNotFound, result->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFormFillingStatusFormNotFound, 1);
}

TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_TabNotAvailable) {
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(Return(
          autofill::FormFillingContextValidationResult::kTabNotAvailable));
  auto* user_data = DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
      web_contents_->GetPrimaryMainFrame());
  std::string doc_token = user_data->serialized_token();
  AttemptOtpFillingTool tool = CreateTool(
      {PageTarget(DomNode{.node_id = 1234, .document_identifier = doc_token})});
  AnnotatedPageContent observation;
  observation.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(doc_token);
  observation.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(1234);

  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, result->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFormFillingStatusTabNotAvailable, 1);
}

TEST_F(AttemptOtpFillingToolTest, Invoke_NoTargetFrameWithOtpFound) {
  EXPECT_CALL(mock_tab(), GetContents()).WillRepeatedly(Return(nullptr));
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOtpTargetFrameNotFound, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kNoTargetFrameWithOtpFound, 1);
}

TEST_F(AttemptOtpFillingToolTest, Invoke_ActorLoginVerificationFailed) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));

  // The confirmation dialog is shown. User declines it.
  webui::mojom::GmailOtpConfirmationResponsePtr response =
      webui::mojom::GmailOtpConfirmationResponse::New(
          /*permission_granted=*/false);
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _))
      .WillOnce(RunOnceCallback<1>(
          webui::mojom::GmailOtpConfirmationResult::NewResponse(
              std::move(response))));

  // OTP should NOT be filled.
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp).Times(0);

  auto verifier =
      std::make_unique<testing::NiceMock<MockActorLoginFlowVerifier>>(
          fake_affiliation_service_);
  EXPECT_CALL(*verifier, VerifyIsActorLoginFlow)
      .WillOnce(
          [](content::FrameTreeNodeId otp_frame_id,
             const url::Origin& otp_frame_origin,
             const url::Origin& main_frame_origin,
             std::optional<url::Origin> context_origin,
             bool should_use_strong_matching,
             base::OnceCallback<std::optional<autofill::ActorLoginContext>()>
                 consume_context_callback,
             base::OnceCallback<void(ActorLoginFlowVerifier::Result)>
                 callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::OnceCallback<
                           std::optional<autofill::ActorLoginContext>()>
                           consume_context_callback,
                       base::OnceCallback<void(ActorLoginFlowVerifier::Result)>
                           callback) {
                      std::move(consume_context_callback).Run();
                      std::move(callback).Run(
                          ActorLoginFlowVerifier::Result::kNoMatch);
                    },
                    std::move(consume_context_callback), std::move(callback)));
          });
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool(
      {target}, /*for_signin=*/true,
      AttemptOtpFillingToolRequest::OtpType::kUnknown, std::move(verifier));
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOtpUserDeclinedFillingConfirmation, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGmailOtpConfirmationDeclinedByUser, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpConfirmationDialogInteractionHistogram,
      GmailOtpConfirmationDialogInteraction::kPermissionDenied, 1);
  histogram_tester_.ExpectBucketCount(kActorOtpVerifyIsActorLoginFlowHistogram,
                                      ActorLoginFlowVerifier::Result::kNoMatch,
                                      1);
}

TEST_F(AttemptOtpFillingToolTest, Invoke_InsecureBeforeFilling) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  // `TimeOfUseValidation()` is called during setup and invokes
  // `ValidateFormFillingContext()`. `Invoke()` calls it a second time during
  // OTP filling. This test specifically tests the call in `Invoke()`.
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure))
      .WillOnce(
          Return(autofill::FormFillingContextValidationResult::kInsecurePage));
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kOtpInsecureContext, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFormFillingNotSecureBeforeFilling, 1);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("TimeOfUse"),
      autofill::FormFillingContextValidationResult::kSecure, 1);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("AfterOtpRetrieved"),
      autofill::FormFillingContextValidationResult::kInsecurePage, 1);
}

TEST_F(AttemptOtpFillingToolTest, Invoke_FormNotFoundBeforeFilling) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      // `TimeOfUseValidation()`
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure))
      // `OnOtpRetrieved()`
      .WillOnce(Return(autofill::FormFillingContextValidationResult::
                           kFormStructureNotFound));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp).Times(0);
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kFormFillingFieldNotFound,
            future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFormNotFoundBeforeFilling, 1);
}

TEST_F(AttemptOtpFillingToolTest,
       Invoke_TabNotAvailableBeforeFillingAfterConfirmation) {
  EXPECT_CALL(delegate().mock_otp_service(), GetLoginContextOrigin())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  // `OnGmailOtpConfirmationResponse()` calls `ValidateFormFillingContext()`,
  // and this test fails that check.
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      // `TimeOfUseValidation()`
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure))
      // `OnOtpRetrieved()`
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure))
      // `OnGmailOtpConfirmationResponse()`
      .WillOnce(Return(
          autofill::FormFillingContextValidationResult::kTabNotAvailable));
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _))
      .WillOnce(RunOnceCallback<1>(
          webui::mojom::GmailOtpConfirmationResult::NewResponse(
              webui::mojom::GmailOtpConfirmationResponse::New(
                  /*permission_granted=*/true))));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp).Times(0);
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target}, /*for_signin=*/true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kTabWentAway, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kTabNotAvailableBeforeFilling, 1);
}

// `Invoke()` returns `kOk` when all conditions are met using a `DomNode`
// target.
TEST_F(AttemptOtpFillingToolTest, Invoke_DomNode_HappyPath) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(true));
  std::string doc_token =
      DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
          web_contents_->GetPrimaryMainFrame())
          ->serialized_token();
  PageTarget target(DomNode{.node_id = 1234, .document_identifier = doc_token});
  AttemptOtpFillingTool tool = CreateTool({target});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
}

TEST_F(AttemptOtpFillingToolTest, Invoke_NoLoginContextAvailable_Approved) {
  EXPECT_CALL(delegate().mock_otp_service(), GetLoginContextOrigin())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  // `ValidateFormFillingContext()` is called once per step: in
  // `TimeOfUseValidation()`, after the OTP is retrieved, and after the
  // confirmation dialog is approved.
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure))
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure))
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure));

  // The confirmation dialog is shown. User approves it.
  webui::mojom::GmailOtpConfirmationResponsePtr response =
      webui::mojom::GmailOtpConfirmationResponse::New(
          /*permission_granted=*/true);
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _))
      .WillOnce(RunOnceCallback<1>(
          webui::mojom::GmailOtpConfirmationResult::NewResponse(
              std::move(response))));

  // OTP should be filled.
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(true));

  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target}, /*for_signin=*/true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFillingOtpSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpConfirmationDialogInteractionHistogram,
      GmailOtpConfirmationDialogInteraction::kPermissionGranted, 1);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("TimeOfUse"),
      autofill::FormFillingContextValidationResult::kSecure, 1);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("AfterOtpRetrieved"),
      autofill::FormFillingContextValidationResult::kSecure, 1);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("AfterConfirmation"),
      autofill::FormFillingContextValidationResult::kSecure, 1);
}

// A failed validation after the user approves the confirmation dialog is
// recorded for that step, with the check that failed.
TEST_F(AttemptOtpFillingToolTest, Invoke_ValidationFailsAfterConfirmation) {
  EXPECT_CALL(delegate().mock_otp_service(), GetLoginContextOrigin())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure))
      .WillOnce(Return(autofill::FormFillingContextValidationResult::kSecure))
      .WillOnce(Return(autofill::FormFillingContextValidationResult::
                           kOtpFieldsFromDifferentOrigins));
  webui::mojom::GmailOtpConfirmationResponsePtr response =
      webui::mojom::GmailOtpConfirmationResponse::New(
          /*permission_granted=*/true);
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _))
      .WillOnce(RunOnceCallback<1>(
          webui::mojom::GmailOtpConfirmationResult::NewResponse(
              std::move(response))));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp).Times(0);

  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target}, /*for_signin=*/true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kOtpInsecureContext, future.Take()->code);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("TimeOfUse"),
      autofill::FormFillingContextValidationResult::kSecure, 1);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("AfterOtpRetrieved"),
      autofill::FormFillingContextValidationResult::kSecure, 1);
  histogram_tester_.ExpectUniqueSample(
      ValidateFormFillingContextHistogram("AfterConfirmation"),
      autofill::FormFillingContextValidationResult::
          kOtpFieldsFromDifferentOrigins,
      1);
}

TEST_F(AttemptOtpFillingToolTest, Invoke_NoLoginContextAvailable_Declined) {
  EXPECT_CALL(delegate().mock_otp_service(), GetLoginContextOrigin())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));

  // The confirmation dialog is shown. User declines it.
  webui::mojom::GmailOtpConfirmationResponsePtr response =
      webui::mojom::GmailOtpConfirmationResponse::New(
          /*permission_granted=*/false);
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _))
      .WillOnce(RunOnceCallback<1>(
          webui::mojom::GmailOtpConfirmationResult::NewResponse(
              std::move(response))));

  // OTP should NOT be filled.
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp).Times(0);

  PageTarget target(gfx::Point(10, 10));
  {
    AttemptOtpFillingTool tool = CreateTool({target}, /*for_signin=*/true);
    SetupSuccessfulTimeOfUseValidation(tool, target);

    TestFuture<ActionResultPtr> future;
    tool.Invoke(future.GetCallback());

    EXPECT_EQ(kOtpUserDeclinedFillingConfirmation, future.Take()->code);
  }
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGmailOtpConfirmationDeclinedByUser, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpConfirmationDialogInteractionHistogram,
      GmailOtpConfirmationDialogInteraction::kPermissionDenied, 1);
  ExpectNoCancellationRecorded(histogram_tester_);
}

TEST_F(AttemptOtpFillingToolTest,
       Invoke_NoLoginContextAvailable_ConfirmationError) {
  EXPECT_CALL(delegate().mock_otp_service(), GetLoginContextOrigin())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));

  // The confirmation dialog returns an error.
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _))
      .WillOnce(RunOnceCallback<1>(
          webui::mojom::GmailOtpConfirmationResult::NewErrorReason(
              webui::mojom::GmailOtpErrorReason::kRequestPromiseNoSubscriber)));

  // OTP should NOT be filled.
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp).Times(0);

  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target}, /*for_signin=*/true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kOtpUnableToFill, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGmailOtpConfirmationResponseNotValid, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpConfirmationDialogInteractionHistogram,
      GmailOtpConfirmationDialogInteraction::kErrorResponse, 1);
}

TEST_F(AttemptOtpFillingToolTest, Invoke_FrameLostDuringVerification) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));

  auto verifier =
      std::make_unique<testing::NiceMock<MockActorLoginFlowVerifier>>(
          fake_affiliation_service_);
  EXPECT_CALL(*verifier, VerifyIsActorLoginFlow)
      .WillOnce(
          [this](
              content::FrameTreeNodeId otp_frame_id,
              const url::Origin& otp_frame_origin,
              const url::Origin& main_frame_origin,
              std::optional<url::Origin> context_origin,
              bool should_use_strong_matching,
              base::OnceCallback<std::optional<autofill::ActorLoginContext>()>
                  consume_context_callback,
              base::OnceCallback<void(ActorLoginFlowVerifier::Result)>
                  callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::OnceCallback<
                           std::optional<autofill::ActorLoginContext>()>
                           consume_context_callback,
                       base::OnceCallback<void(ActorLoginFlowVerifier::Result)>
                           callback,
                       base::OnceClosure simulate_tab_gone) {
                      std::move(consume_context_callback).Run();
                      std::move(simulate_tab_gone).Run();
                      std::move(callback).Run(
                          ActorLoginFlowVerifier::Result::kNoMatch);
                    },
                    std::move(consume_context_callback), std::move(callback),
                    base::BindOnce(
                        [](tabs::MockTabInterface* mock_tab) {
                          EXPECT_CALL(*mock_tab, GetContents())
                              .WillRepeatedly(Return(nullptr));
                        },
                        &mock_tab())));
          });

  // RetrieveOtp should NOT be called.
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp).Times(0);

  PageTarget target(gfx::Point(10, 10));
  {
    AttemptOtpFillingTool tool = CreateTool(
        {target}, /*for_signin=*/true,
        AttemptOtpFillingToolRequest::OtpType::kUnknown, std::move(verifier));
    SetupSuccessfulTimeOfUseValidation(tool, target);

    TestFuture<ActionResultPtr> future;
    tool.Invoke(future.GetCallback());

    EXPECT_EQ(mojom::ActionResultCode::kOtpTargetFrameNotFound,
              future.Take()->code);
  }
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kNoTargetFrameWithOtpFound, 1);
  ExpectNoCancellationRecorded(histogram_tester_);
}

TEST_F(AttemptOtpFillingToolTest,
       Invoke_NoLoginContextAvailable_NullConfirmationResponse) {
  EXPECT_CALL(delegate().mock_otp_service(), GetLoginContextOrigin())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));

  // The confirmation dialog returns a null response.
  webui::mojom::GmailOtpConfirmationResultPtr response;
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog("123456", _))
      .WillOnce(RunOnceCallback<1>(std::move(response)));

  // OTP should NOT be filled.
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp).Times(0);

  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target}, /*for_signin=*/true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kOtpUnableToFill, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGmailOtpConfirmationResponseNotValid, 1);
  histogram_tester_.ExpectUniqueSample(
      kGmailOtpConfirmationDialogInteractionHistogram,
      GmailOtpConfirmationDialogInteraction::kErrorResponse, 1);
}

TEST_F(AttemptOtpFillingToolTest,
       ValidateThenInvoke_OptInPermissionGranted_NewlyOptedInRecorded) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(RunOnceCallback<0>(GmailOtpOptInResult::NewResponse(
          webui::mojom::GmailOtpOptInResponse::New(true))));
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<5>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp)
      .WillOnce(RunOnceCallback<3>(true));

  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target}, /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> validate_future;
  tool.Validate(validate_future.GetCallback());
  EXPECT_EQ(kOk, validate_future.Take()->code);

  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> invoke_future;
  tool.Invoke(invoke_future.GetCallback());
  EXPECT_EQ(kOk, invoke_future.Take()->code);

  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFillingOtpSuccess, 1);
  histogram_tester_.ExpectBucketCount(
      ToolInvocationHistogram("NewlyOptedIn"),
      AttemptOtpFillingToolEvent::kFillingOtpSuccess, 1);
}

}  // namespace actor
