// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/common/compose/compose.mojom.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::EqualsProto;
using base::test::RunOnceCallback;
using testing::_;
using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

namespace {

constexpr char kTypeURL[] =
    "type.googleapis.com/optimization_guide.proto.ComposeResponse";
constexpr char kExampleURL[] = "https://example.com";

class MockModelExecutor
    : public optimization_guide::OptimizationGuideModelExecutor {
 public:
  MOCK_METHOD(void,
              ExecuteModel,
              (optimization_guide::proto::ModelExecutionFeature feature,
               const google::protobuf::MessageLite& request_metadata,
               optimization_guide::OptimizationGuideModelExecutionResultCallback
                   callback));
};

class MockOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL& url,
               optimization_guide::proto::OptimizationType optimization_type,
               optimization_guide::OptimizationGuideDecisionCallback callback));

  MOCK_METHOD(
      optimization_guide::OptimizationGuideDecision,
      CanApplyOptimization,
      (const GURL& url,
       optimization_guide::proto::OptimizationType optimization_type,
       optimization_guide::OptimizationMetadata* optimization_metadata));
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&
                   optimization_types));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>& urls,
       const base::flat_set<optimization_guide::proto::OptimizationType>&
           optimization_types,
       optimization_guide::proto::RequestContext request_context,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
           callback));
};

class MockComposeDialog : public compose::mojom::ComposeDialog {
 public:
  MOCK_METHOD(void,
              ResponseReceived,
              (compose::mojom::ComposeResponsePtr response));
};

}  // namespace

class ChromeComposeClientTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    scoped_feature_list_.InitWithFeatures(
        {compose::features::kEnableCompose,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
    AddTab(browser(), GetPageUrl());
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    client_ = ChromeComposeClient::FromWebContents(contents);
    client_->SetModelExecutorForTest(&model_executor_);
    client_->SetSkipShowDialogForTest();
    client_->SetOptimizationGuideForTest(&opt_guide_);
  }

  void ShowDialogAndBindMojo() { ShowDialogAndBindMojo(base::NullCallback()); }

  void ShowDialogAndBindMojo(ComposeCallback callback) {
    // Show the dialog.
    client().ShowComposeDialog(
        autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
        field_data_, std::nullopt, std::move(callback));

    BindMojo();
  }

  void BindMojo() {
    close_page_handler_.reset();
    page_handler_.reset();
    // Setup Dialog Page Handler.
    mojo::PendingReceiver<compose::mojom::ComposeDialogClosePageHandler>
        close_page_handler_pending_receiver =
            close_page_handler_.BindNewPipeAndPassReceiver();
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler>
        page_handler_pending_receiver =
            page_handler_.BindNewPipeAndPassReceiver();

    // Setup Compose Dialog.
    callback_router_.reset();
    callback_router_ =
        std::make_unique<mojo::Receiver<compose::mojom::ComposeDialog>>(
            &compose_dialog());
    mojo::PendingRemote<compose::mojom::ComposeDialog>
        callback_router_pending_remote =
            callback_router_->BindNewPipeAndPassRemote();

    // Bind mojo to client.
    client_->BindComposeDialog(std::move(close_page_handler_pending_receiver),
                               std::move(page_handler_pending_receiver),
                               std::move(callback_router_pending_remote));
  }

  ChromeComposeClient& client() { return *client_; }
  MockModelExecutor& model_executor() { return model_executor_; }
  MockOptimizationGuideDecider& opt_guide() { return opt_guide_; }
  MockComposeDialog& compose_dialog() { return compose_dialog_; }
  autofill::FormFieldData& field_data() { return field_data_; }

  mojo::Remote<compose::mojom::ComposeDialogClosePageHandler>&
  close_page_handler() {
    return close_page_handler_;
  }

  mojo::Remote<compose::mojom::ComposeDialogPageHandler>& page_handler() {
    return page_handler_;
  }

  GURL GetPageUrl() { return GURL("http://foo/1"); }

  void TearDown() override {
    ClearEnabled();
    client_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void SetEnabled() {
    if (client_ != nullptr) {
      client_->GetComposeEnabling().SetEnabledForTesting();
    }
  }

  void ClearEnabled() {
    if (client_ != nullptr) {
      client_->GetComposeEnabling().ClearEnabledForTesting();
    }
  }

 protected:
  optimization_guide::proto::ComposeRequest ComposeRequest(
      std::string user_input) {
    optimization_guide::proto::ComposePageMetadata page_metadata;
    page_metadata.set_page_url(GetPageUrl().spec());
    page_metadata.set_page_title(base::UTF16ToUTF8(
        browser()->tab_strip_model()->GetWebContentsAt(0)->GetTitle()));

    optimization_guide::proto::ComposeRequest request;
    request.set_user_input(user_input);
    request.set_tone(
        optimization_guide::proto::ComposeTone::COMPOSE_UNSPECIFIED_TONE);
    request.set_length(
        optimization_guide::proto::ComposeLength::COMPOSE_UNSPECIFIED_LENGTH);
    *request.mutable_page_metadata() = std::move(page_metadata);

    return request;
  }

  optimization_guide::proto::ComposeResponse ComposeResponse(
      bool ok,
      std::string output) {
    optimization_guide::proto::ComposeResponse response;
    response.set_output(ok ? output : "");
    return response;
  }

  optimization_guide::proto::Any OptimizationGuideResponse(
      const optimization_guide::proto::ComposeResponse compose_response) {
    optimization_guide::proto::Any any;
    any.set_type_url(kTypeURL);
    compose_response.SerializeToString(any.mutable_value());
    return any;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<ChromeComposeClient> client_;
  MockModelExecutor model_executor_;
  MockOptimizationGuideDecider opt_guide_;
  MockComposeDialog compose_dialog_;
  autofill::FormFieldData field_data_;

  std::unique_ptr<mojo::Receiver<compose::mojom::ComposeDialog>>
      callback_router_;
  mojo::Remote<compose::mojom::ComposeDialogClosePageHandler>
      close_page_handler_;
  mojo::Remote<compose::mojom::ComposeDialogPageHandler> page_handler_;
};

TEST_F(ChromeComposeClientTest, TestCompose) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  compose::mojom::ComposeResponsePtr result = test_future.Take();

  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);
}

TEST_F(ChromeComposeClientTest, TestComposeParams) {
  ShowDialogAndBindMojo();
  std::string user_input = "a user typed this";
  auto matcher = EqualsProto(ComposeRequest(user_input));
  EXPECT_CALL(model_executor(), ExecuteModel(_, matcher, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), user_input);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
}

TEST_F(ChromeComposeClientTest, TestComposeNoResponse) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(base::unexpected(
                optimization_guide::OptimizationGuideModelExecutionError::
                    FromModelExecutionError(
                        optimization_guide::
                            OptimizationGuideModelExecutionError::
                                ModelExecutionError::kGenericFailure)));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kTryAgainLater, result->status);
}

// Tests that we return an error if Optimization Guide is unable to parse the
// response. In this case the response will be absl::nullopt.
TEST_F(ChromeComposeClientTest, TestComposeNoParsedAny) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            optimization_guide::proto::Any any;
            std::move(callback).Run(any);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kTryAgain, result->status);
}

TEST_F(ChromeComposeClientTest, TestOptimizationGuideDisabled) {
  scoped_feature_list_.Reset();

  // Enable Compose and disable optimization guide model execution.
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {optimization_guide::features::kOptimizationGuideModelExecution});

  ShowDialogAndBindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _)).Times(0);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kMisconfiguration, result->status);
}

TEST_F(ChromeComposeClientTest, TestNoModelExecutor) {
  client().SetModelExecutorForTest(nullptr);
  ShowDialogAndBindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _)).Times(0);
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kMisconfiguration, result->status);
}

TEST_F(ChromeComposeClientTest, TestRestoreStateAfterRequestResponse) {
  ShowDialogAndBindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  style_modifiers->tone = compose::mojom::Tone::kCasual;
  style_modifiers->length = compose::mojom::Length::kLonger;
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("", result->compose_state->webui_state);
  EXPECT_FALSE(result->compose_state->response.is_null());
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk,
            result->compose_state->response->status);
  EXPECT_EQ("Cucumbers", result->compose_state->response->result);
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

TEST_F(ChromeComposeClientTest, TestRestoreEmptyState) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("", result->compose_state->webui_state);
  EXPECT_TRUE(result->compose_state->response.is_null());
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

// Tests that saved WebUI state is returned.
TEST_F(ChromeComposeClientTest, TestSaveAndRestoreWebUIState) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> test_future;

  page_handler()->SaveWebUIState("web ui state");
  page_handler()->RequestInitialState(test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = test_future.Take();
  EXPECT_EQ("web ui state", result->compose_state->webui_state);
}

// Tests that same saved WebUI state is returned after compose().
TEST_F(ChromeComposeClientTest, TestSaveThenComposeThenRestoreWebUIState) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr>
      compose_test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            compose_test_future.SetValue(std::move(response));
          }));

  page_handler()->SaveWebUIState("web ui state");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> test_future;
  page_handler()->RequestInitialState(test_future.GetCallback());
  compose::mojom::OpenMetadataPtr open_metadata = test_future.Take();
  EXPECT_EQ("web ui state", open_metadata->compose_state->webui_state);
}

TEST_F(ChromeComposeClientTest, GetOptimizationGuidanceShowNudgeTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED);
  test_metadata.SetAnyMetadataForTesting(compose_hint_metadata);

  EXPECT_CALL(opt_guide(),
              CanApplyOptimization(
                  GURL(kExampleURL),
                  optimization_guide::proto::OptimizationType::COMPOSE,
                  ::testing::An<optimization_guide::OptimizationMetadata*>()))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgPointee<2>(test_metadata),
          testing::Return(
              optimization_guide::OptimizationGuideDecision::kTrue)));
  client().SetOptimizationGuideForTest(&opt_guide());

  // Enable the feature so we can get to the optimization guide call.
  SetEnabled();

  GURL example(kExampleURL);
  compose::ComposeHintDecision decision =
      client().GetOptimizationGuidanceForUrl(example);

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED,
            decision);
}

TEST_F(ChromeComposeClientTest, GetOptimizationGuidanceFeatureOffTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED);
  test_metadata.SetAnyMetadataForTesting(compose_hint_metadata);

  EXPECT_CALL(opt_guide(),
              CanApplyOptimization(
                  GURL(kExampleURL),
                  optimization_guide::proto::OptimizationType::COMPOSE,
                  ::testing::An<optimization_guide::OptimizationMetadata*>()))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgPointee<2>(test_metadata),
          testing::Return(
              optimization_guide::OptimizationGuideDecision::kTrue)));
  client().SetOptimizationGuideForTest(&opt_guide());

  // Disable the feature and check the output is as we expect.
  ClearEnabled();

  GURL example(kExampleURL);
  compose::ComposeHintDecision decision =
      client().GetOptimizationGuidanceForUrl(example);

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_COMPOSE_DISABLED,
      decision);
}

TEST_F(ChromeComposeClientTest, GetOptimizationGuidanceNoFeedbackTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED);
  test_metadata.SetAnyMetadataForTesting(compose_hint_metadata);

  EXPECT_CALL(opt_guide(),
              CanApplyOptimization(
                  GURL(kExampleURL),
                  optimization_guide::proto::OptimizationType::COMPOSE,
                  ::testing::An<optimization_guide::OptimizationMetadata*>()))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgPointee<2>(test_metadata),
          testing::Return(
              optimization_guide::OptimizationGuideDecision::kFalse)));
  client().SetOptimizationGuideForTest(&opt_guide());

  // Enable the feature so we can get to the optimization guide call.
  SetEnabled();

  GURL example(kExampleURL);
  compose::ComposeHintDecision decision =
      client().GetOptimizationGuidanceForUrl(example);

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED,
            decision);
}

TEST_F(ChromeComposeClientTest, GetOptimizationGuidanceNoComposeMetadataTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  test_metadata.SetAnyMetadataForTesting(compose_hint_metadata);

  EXPECT_CALL(opt_guide(),
              CanApplyOptimization(
                  GURL(kExampleURL),
                  optimization_guide::proto::OptimizationType::COMPOSE,
                  ::testing::An<optimization_guide::OptimizationMetadata*>()))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgPointee<2>(test_metadata),
          testing::Return(
              optimization_guide::OptimizationGuideDecision::kTrue)));
  client().SetOptimizationGuideForTest(&opt_guide());

  // Enable the feature so we can get to the optimization guide call.
  SetEnabled();

  GURL example(kExampleURL);
  compose::ComposeHintDecision decision =
      client().GetOptimizationGuidanceForUrl(example);

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED,
            decision);
}

TEST_F(ChromeComposeClientTest, NoStateWorksAtChromeCompose) {
  NavigateAndCommitActiveTab(GURL("chrome://compose"));
  // We skip the dialog showing here, as there is no dialog required at this
  // URL.
  BindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  compose::mojom::ComposeResponsePtr result = test_future.Take();

  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);
}

// Tests that closing after showing the dialog does not crash the browser.
TEST_F(ChromeComposeClientTest, TestCloseUI) {
  ShowDialogAndBindMojo();
  close_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
}

// Tests that closing the session at chrome://compose does not crash the
// browser, even though there is no dialog shown at that URL.
TEST_F(ChromeComposeClientTest, TestCloseUIAtChromeCompose) {
  NavigateAndCommitActiveTab(GURL("chrome://compose"));
  // We skip the dialog showing here, as there is no dialog required at this
  // URL.
  BindMojo();
  close_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
}

// Tests that opening the dialog with user selected text will return that text
// when the WebUI requests initial state.
TEST_F(ChromeComposeClientTest, TestOpenDialogWithSelectedText) {
  field_data().value = u"user selected text";
  field_data().selection_start = 0;
  field_data().selection_end = 18;
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("user selected text", result->initial_input);
}

// Tests that opening the dialog with selected text clears existing state.
TEST_F(ChromeComposeClientTest, TestClearStateWhenOpenWithSelectedText) {
  ShowDialogAndBindMojo();
  page_handler()->SaveWebUIState("web ui state");

  field_data().value = u"user selected text";
  field_data().selection_start = 0;
  field_data().selection_end = 18;
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("", result->compose_state->webui_state);
}

// Tests that undo is not possible when compose is never called and no response
// is ever received.
TEST_F(ChromeComposeClientTest, TestEmptyUndo) {
  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::ComposeStatePtr> test_future;
  page_handler()->Undo(test_future.GetCallback());
  EXPECT_FALSE(test_future.Take());
}

// Tests that Undo is not possible after only one Compose() invocation.
TEST_F(ChromeComposeClientTest, TestUndoUnavailableFirstCompose) {
  ShowDialogAndBindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            compose_future.SetValue(std::move(response));
          }));

  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");
  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available)
      << "First Compose() response should say undo not available.";

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_future;
  page_handler()->RequestInitialState(open_future.GetCallback());
  compose::mojom::OpenMetadataPtr open_metadata = open_future.Take();
  EXPECT_FALSE(open_metadata->compose_state->response->undo_available)
      << "RequestInitialState() should return a response that undo is "
         "not available after only one Compose() invocation.";

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();
  EXPECT_FALSE(state)
      << "Undo should return null after only one Compose() invocation.";
}

// Tests undo after calling Compose() twice.
TEST_F(ChromeComposeClientTest, TestComposeTwiceThenUpdateWebUIStateThenUndo) {
  ShowDialogAndBindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillRepeatedly(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            compose_future.SetValue(std::move(response));
          }));

  page_handler()->SaveWebUIState("this state should be restored with undo");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");
  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";
  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Second Compose() response should "
                                           "say undo is available.";
  page_handler()->SaveWebUIState("user edited the input field further");

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_future;
  page_handler()->RequestInitialState(open_future.GetCallback());
  compose::mojom::OpenMetadataPtr open_metadata = open_future.Take();
  EXPECT_TRUE(open_metadata->compose_state->response->undo_available)
      << "RequestInitialState() should return a response that undo is "
         "available after second Compose() invocation.";
  EXPECT_EQ("user edited the input field further",
            open_metadata->compose_state->webui_state);

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();
  EXPECT_TRUE(state)
      << "Undo should return valid state after second Compose() invocation.";
  EXPECT_EQ("this state should be restored with undo", state->webui_state);
}

// Tests if undo can be done more than once.
TEST_F(ChromeComposeClientTest, TestUndoStackMultipleUndos) {
  ShowDialogAndBindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillRepeatedly(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            compose_future.SetValue(std::move(response));
          }));

  page_handler()->SaveWebUIState("first state");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");
  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");
  page_handler()->SaveWebUIState("third state");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");
  page_handler()->SaveWebUIState("fourth state");

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";
  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Second Compose() response should "
                                           "say undo is available.";
  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Third Compose() response should "
                                           "say undo is available.";

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();
  EXPECT_EQ("second state", state->webui_state);
  EXPECT_TRUE(state->response->undo_available);

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future2;
  page_handler()->Undo(undo_future2.GetCallback());
  compose::mojom::ComposeStatePtr state2 = undo_future2.Take();
  EXPECT_EQ("first state", state2->webui_state);
  EXPECT_FALSE(state2->response->undo_available);
}

// Tests scenario: Undo returns state A. Compose, then undo again returns to
// state A.
TEST_F(ChromeComposeClientTest, TestUndoComposeThenUndoAgain) {
  ShowDialogAndBindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillRepeatedly(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            compose_future.SetValue(std::move(response));
          }));

  page_handler()->SaveWebUIState("first state");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");
  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");
  page_handler()->SaveWebUIState("wip web ui state");

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";
  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Second Compose() response should "
                                           "say undo is available.";

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  EXPECT_EQ("first state", undo_future.Take()->webui_state);

  page_handler()->SaveWebUIState("third state");
  page_handler()->Compose(compose::mojom::StyleModifiers::New(), "");

  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Third Compose() response should "
                                           "say undo is available.";

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo2_future;
  page_handler()->Undo(undo2_future.GetCallback());
  EXPECT_EQ("first state", undo2_future.Take()->webui_state);
}

// Tests that the callback is run when AcceptComposeResponse is called.
TEST_F(ChromeComposeClientTest, TestAcceptComposeResultCallback) {
  base::test::TestFuture<const std::u16string&> accept_callback;
  ShowDialogAndBindMojo(accept_callback.GetCallback());

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")));
          })));
  EXPECT_CALL(compose_dialog(), ResponseReceived(_));

  // Before Compose is called AcceptComposeResult will return false.
  base::test::TestFuture<bool> accept_future_1;
  page_handler()->AcceptComposeResult(accept_future_1.GetCallback());
  EXPECT_EQ(false, accept_future_1.Take());

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  base::test::TestFuture<bool> accept_future_2;
  page_handler()->AcceptComposeResult(accept_future_2.GetCallback());
  EXPECT_EQ(true, accept_future_2.Take());

  // Check that the original callback from Autofill was called correctly.
  EXPECT_EQ(u"Cucumbers", accept_callback.Take());
}

TEST_F(ChromeComposeClientTest, ThumbsDownOpensCorrectURL) {
  GURL bug_url("https://goto.google.com/ccbrfd");

  ShowDialogAndBindMojo();

  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());
  page_handler()->OpenBugReportingLink();

  // Wait for the resulting new tab to be created.
  tab_add_waiter.Wait();
  // Check that the new foreground tab is opened.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // Check expected URL of the new tab.
  content::WebContents* new_tab_webcontents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(bug_url, new_tab_webcontents->GetVisibleURL());
}

#if defined(GTEST_HAS_DEATH_TEST)
// Tests that the Compose client crashes the browser if a webcontents
// tries to bind mojo without opening the dialog at a non Compose URL.
TEST_F(ChromeComposeClientTest, NoStateCrashesAtOtherUrls) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  // We skip the dialog showing here, to validate that non special URLs check.
  EXPECT_DEATH(BindMojo(), "");
}

// Tests that the Compose client crashes the browser if a webcontents
// sends any message when the dialog has not been shown.
TEST_F(ChromeComposeClientTest, TestCannotSendMessagesToNotShownDialog) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH(page_handler()->SaveWebUIState(""), "");
}

// Tests that the Compose client crashes the browser if a webcontents
// tries to close the dialog when the dialog has not been shown.
TEST_F(ChromeComposeClientTest, TestCannotCloseNotShownDialog) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH(
      close_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton),
      "");
}

// Tests that the Compose client crashes the browser if a webcontents
// tries to close the dialog when the dialog has not been shown.
TEST_F(ChromeComposeClientTest, TestCannotSendMessagesAfterClosingDialog) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  ShowDialogAndBindMojo();
  close_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
  // Any message after closing the session will crash.
  EXPECT_DEATH(page_handler()->SaveWebUIState(""), "");
}

// Tests that the Compose client crashes the browser if a webcontents
// sends any more messages after closing the dialog at chrome://contents.
TEST_F(ChromeComposeClientTest,
       TestCannotSendMessagesAfterClosingDialogAtChromeCompose) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  NavigateAndCommitActiveTab(GURL("chrome://compose"));
  // We skip the dialog showing here, as there is no dialog required at this
  // URL.
  BindMojo();
  close_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
  // Any message after closing the session will crash.
  EXPECT_DEATH(page_handler()->SaveWebUIState(""), "");
}
#endif  // GTEST_HAS_DEATH_TEST
