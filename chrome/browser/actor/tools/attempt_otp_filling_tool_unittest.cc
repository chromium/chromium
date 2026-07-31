// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/tools/actor_login_flow_verifier.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_metrics.h"
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

  MOCK_METHOD(autofill::FormFillingContextStatus,
              ValidateFormFillingContext,
              (tabs::TabHandle, base::span<const autofill::FieldGlobalId>),
              (const, override));
  MOCK_METHOD(
      void,
      OnPasswordFillingStarted,
      (tabs::TabHandle, const url::Origin&, bool, base::span<const int>),
      (override));
  MOCK_METHOD(void, AbortLoginTracking, (), (override));
  MOCK_METHOD(std::optional<autofill::ActorLoginContext>,
              ConsumeLoginContext,
              (),
              (override));
  MOCK_METHOD(
      void,
      RetrieveOtp,
      (tabs::TabHandle,
       const url::Origin&,
       const std::vector<autofill::FieldGlobalId>&,
       bool,
       base::OnceCallback<
           void(base::expected<std::string,
                               one_time_tokens::OneTimeTokenRetrievalError>)>),
      (override));
  MOCK_METHOD(void,
              FillOtp,
              (tabs::TabHandle,
               const std::vector<autofill::FieldGlobalId>&,
               const std::string&,
               base::OnceCallback<void(bool)>),
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
               const std::optional<autofill::ActorLoginContext>& context,
               base::OnceCallback<void(bool)> callback),
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
  void AddTab(
      tabs::TabHandle tab_handle,
      bool stop_task_on_detach,
      base::OnceCallback<void(mojom::ActionResultPtr)> callback) override {}
  bool HasTab(tabs::TabHandle tab_handle) override { return false; }
  void RemoveTab(tabs::TabHandle tab_handle) override {}
  void FailCurrentTool(mojom::ActionResultCode reason) override {}
  base::WeakPtr<actor_login::ActionSequenceDelegate> GetActionSequenceDelegate()
      override {
    return nullptr;
  }

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

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    delegate_ = std::make_unique<FakeToolDelegate>(profile_.get());
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents_, GURL("https://example.com"));
    EXPECT_CALL(*mock_tab_, GetContents())
        .WillRepeatedly(Return(web_contents_));
    EXPECT_CALL(delegate_->mock_otp_service(), ValidateFormFillingContext)
        .WillRepeatedly(Return(autofill::FormFillingContextStatus::kSecure));
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
    return AttemptOtpFillingTool(TaskId(1), delegate(), mock_tab().GetHandle(),
                                 std::move(trigger_fields), for_signin,
                                 predicted_otp_type, std::move(verifier));
  }

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
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  affiliations::FakeAffiliationService fake_affiliation_service_;
  std::unique_ptr<FakeToolDelegate> delegate_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
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
using mojom::ActionResultCode::kOtpNoLastTabObservation;
using mojom::ActionResultCode::kOtpRetrievalError;
using mojom::ActionResultCode::kOtpTargetFrameNotFound;
using mojom::ActionResultCode::kOtpUnableToFill;
using mojom::ActionResultCode::kOtpUserDeclinedOptingIntoFilling;
using mojom::ActionResultCode::kTabWentAway;
using mojom::ActionResultCode::kToolTimeout;
using optimization_guide::DocumentIdentifierUserData;
using optimization_guide::proto::AnnotatedPageContent;
using webui::mojom::GmailOtpErrorReason;
using webui::mojom::GmailOtpOptInResult;
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
  histogram_tester_.ExpectBucketCount(kGmailOtpOptInCardInteractionHistogram,
                                      GmailOtpOptInCardInteraction::kShowCard,
                                      1);
  histogram_tester_.ExpectBucketCount(
      kGmailOtpOptInCardInteractionHistogram,
      GmailOtpOptInCardInteraction::kPermissionGranted, 1);
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
  histogram_tester_.ExpectBucketCount(kGmailOtpOptInCardInteractionHistogram,
                                      GmailOtpOptInCardInteraction::kShowCard,
                                      1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kOptInErrorResponse, 1);
  histogram_tester_.ExpectBucketCount(kGmailOtpOptInCardInteractionHistogram,
                                      GmailOtpOptInCardInteraction::kShowCard,
                                      1);
  histogram_tester_.ExpectBucketCount(
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
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kStartFillingAttempt, 1);
}

// Time of use validation returns kTabWentAway when the target tab is not
// available any more.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_TabWentAway) {
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
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
      .WillOnce(Return(autofill::FormFillingContextStatus::kInsecureContext));
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
}

// `Invoke()` returns `kOk` when all conditions are met.
TEST_F(AttemptOtpFillingToolTest, Invoke_HappyPath) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));
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

// `Invoke()` fails with `kOtpFillFailure` when the result of
// filling the OTP is false.
TEST_F(AttemptOtpFillingToolTest, Invoke_ErrorFilling) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));
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

// `Invoke()` fails with `OneTimeTokenRetrievalError::kUnknown` when retrieving
// the OTP fails.
TEST_F(AttemptOtpFillingToolTest, Invoke_ErrorRetrievingGmailOtp) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>(base::unexpected(
          one_time_tokens::OneTimeTokenRetrievalError::kUnknown)));
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  ActionResultPtr action_result = future.Take();
  EXPECT_EQ(kOtpRetrievalError, action_result->code);
  EXPECT_EQ("An error occurred during OTP retrieval: 0",
            action_result->message);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kOtpRetrievalError, 1);
}

// `Invoke()` fails with `kOtpRetrievalTimeout` when the OTP retrieval times
// out.
TEST_F(AttemptOtpFillingToolTest, Invoke_TimeoutRetrievingGmailOtp) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>(base::unexpected(
          one_time_tokens::OneTimeTokenRetrievalError::kSubscriptionExpired)));
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool({target});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  ActionResultPtr action_result = future.Take();
  EXPECT_EQ(mojom::ActionResultCode::kOtpRetrievalTimeout, action_result->code);
  EXPECT_EQ("OTP retrieval timed out.", action_result->message);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kOtpRetrievalError, 1);
}

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
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kOptInNullResponse, 1);
  histogram_tester_.ExpectBucketCount(kGmailOtpOptInCardInteractionHistogram,
                                      GmailOtpOptInCardInteraction::kShowCard,
                                      1);
  histogram_tester_.ExpectBucketCount(
      kGmailOtpOptInCardInteractionHistogram,
      GmailOtpOptInCardInteraction::kErrorResponse, 1);
}

TEST_F(AttemptOtpFillingToolTest, Validate_GmailOtpFillingEnabled) {
  AttemptOtpFillingTool tool = CreateTool({PageTarget(gfx::Point(10, 10))});
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
}

TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_FormNotFound) {
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(Return(autofill::FormFillingContextStatus::kFormNotFound));
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
      .WillOnce(Return(autofill::FormFillingContextStatus::kTabNotAvailable));
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
      .WillOnce(RunOnceCallback<4>("123456"));

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
      .WillOnce(base::test::RunOnceCallback<3>(false));
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool(
      {target}, /*for_signin=*/true,
      AttemptOtpFillingToolRequest::OtpType::kUnknown, std::move(verifier));
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kOtpUserDeclinedOptingIntoFilling,
            future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGmailOtpConfirmationDeclinedByUser, 1);
}

TEST_F(AttemptOtpFillingToolTest,
       Invoke_ActorLoginVerificationFailed_WithBypassSwitch) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAttemptOtpFillingBypassLoginCheck);
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(true));
  EXPECT_CALL(delegate(), RequestToShowGmailOtpConfirmationDialog).Times(0);
  auto verifier =
      std::make_unique<testing::NiceMock<MockActorLoginFlowVerifier>>(
          fake_affiliation_service_);
  EXPECT_CALL(*verifier, VerifyIsActorLoginFlow)
      .WillOnce(base::test::RunOnceCallback<3>(false));
  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool(
      {target}, /*for_signin=*/true,
      AttemptOtpFillingToolRequest::OtpType::kUnknown, std::move(verifier));
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kFillingOtpSuccess, 1);
}

TEST_F(AttemptOtpFillingToolTest, Invoke_InsecureBeforeFilling) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));
  // `TimeOfUseValidation()` is called during setup and invokes
  // `ValidateFormFillingContext()`. `Invoke()` calls it a second time during
  // OTP filling. This test specifically tests the call in `Invoke()`.
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(Return(autofill::FormFillingContextStatus::kSecure))
      .WillOnce(Return(autofill::FormFillingContextStatus::kInsecureContext));
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
}

// `Invoke()` returns `kOk` when all conditions are met using a `DomNode`
// target.
TEST_F(AttemptOtpFillingToolTest, Invoke_DomNode_HappyPath) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));
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
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));
  // ValidateFormFillingContext is called twice: once in TimeOfUseValidation,
  // once before FillOtp
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillRepeatedly(Return(autofill::FormFillingContextStatus::kSecure));

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
}

TEST_F(AttemptOtpFillingToolTest, Invoke_NoLoginContextAvailable_Declined) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));

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
  AttemptOtpFillingTool tool = CreateTool({target}, /*for_signin=*/true);
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kOtpUserDeclinedOptingIntoFilling,
            future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kGmailOtpConfirmationDeclinedByUser, 1);
}

TEST_F(AttemptOtpFillingToolTest,
       Invoke_NoLoginContextAvailable_ConfirmationError) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));

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
}

TEST_F(AttemptOtpFillingToolTest, Invoke_FrameLostDuringVerification) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));

  auto verifier =
      std::make_unique<testing::NiceMock<MockActorLoginFlowVerifier>>(
          fake_affiliation_service_);
  EXPECT_CALL(*verifier, VerifyIsActorLoginFlow)
      .WillOnce([this](
                    content::FrameTreeNodeId otp_frame_id,
                    const url::Origin& otp_frame_origin,
                    const std::optional<autofill::ActorLoginContext>& context,
                    base::OnceCallback<void(bool)> callback) {
        // Simulate tab/contents going away during verification.
        EXPECT_CALL(mock_tab(), GetContents()).WillRepeatedly(Return(nullptr));
        std::move(callback).Run(false);
      });

  // RetrieveOtp should NOT be called.
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp).Times(0);

  PageTarget target(gfx::Point(10, 10));
  AttemptOtpFillingTool tool = CreateTool(
      {target}, /*for_signin=*/true,
      AttemptOtpFillingToolRequest::OtpType::kUnknown, std::move(verifier));
  SetupSuccessfulTimeOfUseValidation(tool, target);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(mojom::ActionResultCode::kOtpTargetFrameNotFound,
            future.Take()->code);
  histogram_tester_.ExpectBucketCount(
      kAttemptOtpFillingToolHistogram,
      AttemptOtpFillingToolEvent::kNoTargetFrameWithOtpFound, 1);
}

TEST_F(AttemptOtpFillingToolTest,
       Invoke_NoLoginContextAvailable_NullConfirmationResponse) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<4>("123456"));

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
}

}  // namespace actor
