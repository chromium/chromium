// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/common/compose/compose.mojom.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/proto/compose_metadata.pb.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
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

namespace {

constexpr char kTypeURL[] = "type.googleapis.com/compose_proto.ComposeResponse";
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

  void ShowDialogAndBindMojo() {
    // Show the dialog.
    client().ShowComposeDialog(
        autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
        autofill::FormFieldData(), std::nullopt, base::NullCallback());

    BindMojo();
  }

  void BindMojo() {
    // Setup Dialog Page Handler.
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler>
        page_handler_pending_receiver =
            page_handler_.BindNewPipeAndPassReceiver();

    // Setup Compose Dialog.
    callback_router_ =
        std::make_unique<mojo::Receiver<compose::mojom::ComposeDialog>>(
            &compose_dialog());
    mojo::PendingRemote<compose::mojom::ComposeDialog>
        callback_router_pending_remote =
            callback_router_->BindNewPipeAndPassRemote();

    // Bind mojo to client.
    client_->BindComposeDialog(std::move(page_handler_pending_receiver),
                               std::move(callback_router_pending_remote));
  }

  ChromeComposeClient& client() { return *client_; }
  MockModelExecutor& model_executor() { return model_executor_; }
  MockOptimizationGuideDecider& opt_guide() { return opt_guide_; }
  MockComposeDialog& compose_dialog() { return compose_dialog_; }

  mojo::Remote<compose::mojom::ComposeDialogPageHandler>& page_handler() {
    return page_handler_;
  }

  GURL GetPageUrl() { return GURL("http://foo/1"); }

  void TearDown() override {
    client_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void SetEnabled() { ComposeEnabling::SetEnabledForTesting(); }

  void ClearEnabled() { ComposeEnabling::ClearEnabledForTesting(); }

 protected:
  compose_proto::ComposeRequest ComposeRequest(std::string user_input) {
    compose_proto::ComposePageMetadata page_metadata;
    page_metadata.set_page_url(GetPageUrl().spec());
    page_metadata.set_page_title(base::UTF16ToUTF8(
        browser()->tab_strip_model()->GetWebContentsAt(0)->GetTitle()));

    compose_proto::ComposeRequest request;
    request.set_user_input(user_input);
    request.set_tone(compose_proto::ComposeTone::COMPOSE_UNSPECIFIED_TONE);
    request.set_length(
        compose_proto::ComposeLength::COMPOSE_UNSPECIFIED_LENGTH);
    *request.mutable_page_metadata() = std::move(page_metadata);

    return request;
  }

  compose_proto::ComposeResponse ComposeResponse(bool ok) {
    compose_proto::ComposeResponse response;
    response.set_output(ok ? "Cucumbers" : "");
    return response;
  }

  optimization_guide::proto::Any OptimizationGuideResponse(
      const compose_proto::ComposeResponse compose_response) {
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

  std::unique_ptr<mojo::Receiver<compose::mojom::ComposeDialog>>
      callback_router_;
  mojo::Remote<compose::mojom::ComposeDialogPageHandler> page_handler_;
};

TEST_F(ChromeComposeClientTest, TestCompose) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true)));
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
                OptimizationGuideResponse(ComposeResponse(true)));
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
                  callback) { std::move(callback).Run(absl::nullopt); })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this");

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kError, result->status);
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
  EXPECT_EQ(compose::mojom::ComposeStatus::kError, result->status);
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
  EXPECT_EQ(compose::mojom::ComposeStatus::kError, result->status);
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
  EXPECT_EQ(compose::mojom::ComposeStatus::kError, result->status);
}

TEST_F(ChromeComposeClientTest, TestRestoreStateAfterRequestResponse) {
  ShowDialogAndBindMojo();

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true)));
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

TEST_F(ChromeComposeClientTest, TestSaveAndRestoreWebUIState) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> test_future;

  page_handler()->SaveWebUIState("web ui state");
  page_handler()->RequestInitialState(test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = test_future.Take();
  EXPECT_EQ("web ui state", result->compose_state->webui_state);
}

TEST_F(ChromeComposeClientTest, GetOptimizationGuidanceShowNudgeTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(compose::ComposeNudgeDecision::SHOW_NUDGE);
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
  compose::ComposeNudgeDecision decision =
      client().GetOptimizationGuidanceForUrl(example);
  ClearEnabled();

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeNudgeDecision::SHOW_NUDGE, decision);
}

TEST_F(ChromeComposeClientTest, GetOptimizationGuidanceFeatureOffTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(compose::ComposeNudgeDecision::SHOW_NUDGE);
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
  compose::ComposeNudgeDecision decision =
      client().GetOptimizationGuidanceForUrl(example);
  ClearEnabled();

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeNudgeDecision::COMPOSE_DISABLED, decision);
}

TEST_F(ChromeComposeClientTest, GetOptimizationGuidanceNoFeedbackTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(compose::ComposeNudgeDecision::SHOW_NUDGE);
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
  compose::ComposeNudgeDecision decision =
      client().GetOptimizationGuidanceForUrl(example);
  ClearEnabled();

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeNudgeDecision::UNKNOWN, decision);
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
  compose::ComposeNudgeDecision decision =
      client().GetOptimizationGuidanceForUrl(example);
  ClearEnabled();

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeNudgeDecision::UNKNOWN, decision);
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
                OptimizationGuideResponse(ComposeResponse(true)));
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

#if defined(GTEST_HAS_DEATH_TEST)
// Tests that the Compose client crashes the browser if a webcontents
// tries to bind mojo without opening the dialog at a non Compose URL.
TEST_F(ChromeComposeClientTest, NoStateCrashesAtOtherUrls) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  // We skip the dialog showing here, to validate that non special URLs check.
  EXPECT_DEATH(BindMojo(), "");
}
#endif  // GTEST_HAS_DEATH_TEST
