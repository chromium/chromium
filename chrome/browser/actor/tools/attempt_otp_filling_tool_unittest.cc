// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/core/task_id.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
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
       const std::vector<autofill::FieldGlobalId>&,
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

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<FakeToolDelegate> delegate_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
};

namespace {  // Have commonly used namespaces here to make tests more readable
using autofill::prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp;
using autofill::prefs::IsAutofillGmailOtpFillingEnabled;
using autofill::prefs::SetAutofillGmailOtpFillingActivationDismissalTimestamp;
using autofill::prefs::SetAutofillGmailOtpFillingEnabled;
using base::test::RunOnceCallback;
using base::test::TestFuture;
using mojom::ActionResultPtr;
using mojom::ActionResultCode::kFormFillingAutofillUnavailable;
using mojom::ActionResultCode::kFormFillingDialogError;
using mojom::ActionResultCode::kFormFillingFieldNotFound;
using mojom::ActionResultCode::kFormFillingNoLastTabObservation;
using mojom::ActionResultCode::kFormFillingUnknownAutofillError;
using mojom::ActionResultCode::kOk;
using mojom::ActionResultCode::kOtpFillFailure;
using mojom::ActionResultCode::kOtpRetrievalError;
using mojom::ActionResultCode::kTabWentAway;
using mojom::ActionResultCode::kToolTimeout;
using optimization_guide::DocumentIdentifierUserData;
using optimization_guide::proto::AnnotatedPageContent;
using webui::mojom::GmailOtpOptInErrorReason;
using webui::mojom::GmailOtpOptInResult;
}  // namespace

// When the user grants permission to opt in to Gmail OTPs, the tool
// successfully validates, and persists the decision in the prefs, including to
// remove the dismissal timestamp.
TEST_F(AttemptOtpFillingToolTest, Validate_OptInPermissionGranted) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(
          RunOnceCallback<0>(GmailOtpOptInResult::NewPermissionGranted(true)));
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
  EXPECT_TRUE(IsAutofillGmailOtpFillingEnabled(prefs()));
  EXPECT_EQ(base::Time(),
            GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs()));
}

// When the user denies permission to opt in to Gmail OTPs, the tool
// fails validation, and persists the decision in the dismissal timestamp pref
// without turning on Gmail OTP.
TEST_F(AttemptOtpFillingToolTest, Validate_OptInPermissionDenied) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(
          RunOnceCallback<0>(GmailOtpOptInResult::NewPermissionGranted(false)));
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kFormFillingAutofillUnavailable, future.Take()->code);
  EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(prefs()));
  EXPECT_EQ(base::Time::Now(),
            GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs()));
}

// When the asking the user for opting into using Gmail OTPs fails, the tool
// fails validation, and doesn't modify any preferences.
TEST_F(AttemptOtpFillingToolTest, Validate_OptInPermissionCallbackError) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog)
      .WillOnce(RunOnceCallback<0>(GmailOtpOptInResult::NewErrorReason(
          GmailOtpOptInErrorReason::kRequestPromiseNoSubscriber)));
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kFormFillingDialogError, future.Take()->code);
  EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(prefs()));
  EXPECT_EQ(base::Time::Now() - base::Days(90),
            GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs()));
}

// When Gmail OTP is disabled, but we're within the cool off period for asking
// the user for opting into using Gmail OTPs, the tool fails validation and the
// persisted prefs did not change.
TEST_F(AttemptOtpFillingToolTest,
       Validate_GmailOtpFillingDisabledWithinCoolOffPeriod) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog).Times(0);
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp).Times(0);
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(89));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  EXPECT_EQ(kFormFillingAutofillUnavailable, future.Take()->code);
  EXPECT_FALSE(IsAutofillGmailOtpFillingEnabled(prefs()));
  EXPECT_EQ(base::Time::Now() - base::Days(89),
            GetAutofillGmailOtpFillingActivationDismissalTimestamp(prefs()));
}

// When Gmail OTP is disabled, and we're outside the cool off period for asking
// the user for opting into using Gmail OTPs, the tool asks the user for opting
// into using Gmail OTPs via RequestToShowGmailOtpOptInDialog.
TEST_F(AttemptOtpFillingToolTest,
       Validate_GmailOtpFillingDisabledOutsideCoolOffPeriod) {
  EXPECT_CALL(delegate(), RequestToShowGmailOtpOptInDialog);
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), false);
  SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs(), base::Time::Now() - base::Days(90));

  TestFuture<ActionResultPtr> future;
  tool.Validate(future.GetCallback());

  // expect call to RequestToShowGmailOtpOptInDialog, see EXPECT_CALL above
}

// Time of use validation returns kTabWentAway when the target tab is not
// available any more.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_TabWentAway) {
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  mock_tab_.reset();

  AnnotatedPageContent observation;
  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(kTabWentAway, result->code);
}

// Time of use validation returns kFormFillingNoLastTabObservation when a tool
// invocation request is dispatched without a preceding observation. This might
// be a result of the tab not having been observed (yet), a possible GLIC state.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_NoLastObservation) {
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);

  ActionResultPtr result = tool.TimeOfUseValidation(nullptr);

  EXPECT_EQ(kFormFillingNoLastTabObservation, result->code);
}

// Time of use validation returns kFormFillingFieldNotFound when any of the
// trigger fields aren't found.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_FieldNotFound) {
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);

  AnnotatedPageContent observation;
  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(kFormFillingFieldNotFound, result->code);
}

// Time of use validation returns kOk when all conditions are met.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_HappyPath) {
  auto* user_data = DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
      web_contents_->GetPrimaryMainFrame());
  std::string doc_token = user_data->serialized_token();
  AttemptOtpFillingTool tool(
      TaskId(1), delegate(), mock_tab().GetHandle(),
      {PageTarget(DomNode{.node_id = 1234, .document_identifier = doc_token})},
      /*for_signin=*/true);
  AnnotatedPageContent observation;
  observation.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(doc_token);
  observation.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(1234);

  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(kOk, result->code);
}

// Time of use validation returns `kOtpInsecureContext` when form filling
// context is insecure.
TEST_F(AttemptOtpFillingToolTest, TimeOfUseValidation_InsecureContext) {
  EXPECT_CALL(delegate().mock_otp_service(), ValidateFormFillingContext)
      .WillOnce(Return(autofill::FormFillingContextStatus::kInsecureContext));
  auto* user_data = DocumentIdentifierUserData::GetOrCreateForCurrentDocument(
      web_contents_->GetPrimaryMainFrame());
  std::string doc_token = user_data->serialized_token();
  AttemptOtpFillingTool tool(
      TaskId(1), delegate(), mock_tab().GetHandle(),
      {PageTarget(DomNode{.node_id = 1234, .document_identifier = doc_token})},
      /*for_signin=*/true);
  AnnotatedPageContent observation;
  observation.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(doc_token);
  observation.mutable_root_node()
      ->mutable_content_attributes()
      ->set_common_ancestor_dom_node_id(1234);

  ActionResultPtr result = tool.TimeOfUseValidation(&observation);

  EXPECT_EQ(mojom::ActionResultCode::kOtpInsecureContext, result->code);
}

// Invoke() returns kOk when all conditions are met.
TEST_F(AttemptOtpFillingToolTest, Invoke_HappyPath) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<2>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(true));
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOk, future.Take()->code);
}

// Invoke() fails with kOtpFillFailure when the result of
// filling the OTP is false.
TEST_F(AttemptOtpFillingToolTest, Invoke_ErrorFilling) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<2>("123456"));
  EXPECT_CALL(delegate().mock_otp_service(), FillOtp(_, _, "123456", _))
      .WillOnce(RunOnceCallback<3>(false));
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOtpFillFailure, future.Take()->code);
}

// Invoke() fails with kFormFillingError when retrieving the OTP fails.
TEST_F(AttemptOtpFillingToolTest, Invoke_ErrorRetrievingGmailOtp) {
  EXPECT_CALL(delegate().mock_otp_service(), ConsumeLoginContext())
      .WillOnce(Return(CreateValidLoginContext()));
  EXPECT_CALL(delegate().mock_otp_service(), RetrieveOtp)
      .WillOnce(RunOnceCallback<2>(base::unexpected(
          one_time_tokens::OneTimeTokenRetrievalError::kUnknown)));
  AttemptOtpFillingTool tool(TaskId(1), delegate(), mock_tab().GetHandle(),
                             {PageTarget(gfx::Point(10, 10))},
                             /*for_signin=*/true);
  SetAutofillGmailOtpFillingEnabled(prefs(), true);

  TestFuture<ActionResultPtr> future;
  tool.Invoke(future.GetCallback());

  EXPECT_EQ(kOtpRetrievalError, future.Take()->code);
}

}  // namespace actor
