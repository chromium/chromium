// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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
using testing::_;

namespace {

constexpr char kTypeURL[] = "type.googleapis.com/compose_proto.ComposeResponse";

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

    // Setup Dialog Page Handler.
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler>
        page_handler_pending_receiver =
            page_handler_.BindNewPipeAndPassReceiver();

    // Setup Compose Dialog.
    compose::mojom::ComposeDialog dialog_stub;
    mojo::Receiver<compose::mojom::ComposeDialog> callback_router(&dialog_stub);
    mojo::PendingRemote<compose::mojom::ComposeDialog>
        callback_router_pending_remote =
            callback_router.BindNewPipeAndPassRemote();

    // Bind mojo to client.
    client_->BindComposeDialog(std::move(page_handler_pending_receiver),
                               std::move(callback_router_pending_remote));
  }

  ChromeComposeClient& client() { return *client_; }
  MockModelExecutor& model_executor() { return model_executor_; }
  mojo::Remote<compose::mojom::ComposeDialogPageHandler>& page_handler() {
    return page_handler_;
  }

  GURL GetPageUrl() { return GURL("http://foo/1"); }

  void TearDown() override {
    client_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

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

  mojo::Remote<compose::mojom::ComposeDialogPageHandler> page_handler_;
};

TEST_F(ChromeComposeClientTest, TestCompose) {
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true)));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this",
                          test_future.GetCallback());

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);
}

TEST_F(ChromeComposeClientTest, TestComposeParams) {
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

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), user_input,
                          test_future.GetCallback());

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
}

TEST_F(ChromeComposeClientTest, TestComposeNoResponse) {
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) { std::move(callback).Run(absl::nullopt); })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this",
                          test_future.GetCallback());

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kError, result->status);
}

// Tests that we return an error if Optimization Guide is unable to parse the
// response. In this case the response will be absl::nullopt.
TEST_F(ChromeComposeClientTest, TestComposeNoParsedAny) {
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            optimization_guide::proto::Any any;
            std::move(callback).Run(any);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;

  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this",
                          test_future.GetCallback());

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kError, result->status);
}

TEST_F(ChromeComposeClientTest, TestOptimizationGuideDisabled) {
  scoped_feature_list_.Reset();

  // Disable optimization guide.
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {optimization_guide::features::kOptimizationGuideModelExecution});

  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _)).Times(0);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this",
                          test_future.GetCallback());

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kError, result->status);
}

TEST_F(ChromeComposeClientTest, TestNoModelExecutor) {
  client().SetModelExecutorForTest(nullptr);
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _)).Times(0);
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  auto style_modifiers = compose::mojom::StyleModifiers::New();
  page_handler()->Compose(std::move(style_modifiers), "a user typed this",
                          test_future.GetCallback());

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kError, result->status);
}

TEST_F(ChromeComposeClientTest, TestRestoreStateAfterRequestResponse) {
  EXPECT_CALL(model_executor(), ExecuteModel(_, _, _))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true)));
          })));

  client().ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      autofill::FormFieldData(), std::nullopt, base::NullCallback());

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  auto style_modifiers = compose::mojom::StyleModifiers::New();
  style_modifiers->tone = compose::mojom::Tone::kCasual;
  style_modifiers->length = compose::mojom::Length::kLonger;
  page_handler()->Compose(std::move(style_modifiers), "a user typed this",
                          test_future.GetCallback());

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("a user typed this", result->compose_state->input);
  EXPECT_EQ(compose::mojom::Tone::kCasual, result->compose_state->style->tone);
  EXPECT_EQ(compose::mojom::Length::kLonger,
            result->compose_state->style->length);
  EXPECT_EQ(false, result->compose_state->response.is_null());
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk,
            result->compose_state->response->status);
  EXPECT_EQ("Cucumbers", result->compose_state->response->result);
  EXPECT_EQ(false, result->compose_state->has_pending_request);
}

TEST_F(ChromeComposeClientTest, TestRestoreEmptyState) {
  client().ShowComposeDialog(
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup,
      autofill::FormFieldData(), std::nullopt, base::NullCallback());

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("", result->compose_state->input);
  EXPECT_EQ(compose::mojom::Tone::kUnset, result->compose_state->style->tone);
  EXPECT_EQ(compose::mojom::Length::kUnset,
            result->compose_state->style->length);
  EXPECT_EQ(true, result->compose_state->response.is_null());
}
