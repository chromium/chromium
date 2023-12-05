// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/common/compose/compose.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using base::test::RunOnceCallback;
using testing::_;
using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;

namespace {

const uint64_t kSessionIdHigh = 1234;
const uint64_t kSessionIdLow = 5678;
constexpr char kTypeURL[] =
    "type.googleapis.com/optimization_guide.proto.ComposeResponse";

class MockModelExecutor
    : public optimization_guide::OptimizationGuideModelExecutor {
 public:
  MOCK_METHOD(std::unique_ptr<Session>,
              StartSession,
              (optimization_guide::proto::ModelExecutionFeature feature));
  MOCK_METHOD(void,
              ExecuteModel,
              (optimization_guide::proto::ModelExecutionFeature feature,
               const google::protobuf::MessageLite& request_metadata,
               optimization_guide::OptimizationGuideModelExecutionResultCallback
                   callback));
};
class MockModelQualityLogsUploader
    : public optimization_guide::ModelQualityLogsUploader {
 public:
  MOCK_METHOD(
      void,
      UploadModelQualityLogs,
      (std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry));
};

class MockSession
    : public optimization_guide::OptimizationGuideModelExecutor::Session {
 public:
  MOCK_METHOD(void,
              AddContext,
              (const google::protobuf::MessageLite& request_metadata));
  MOCK_METHOD(
      void,
      ExecuteModel,
      (const google::protobuf::MessageLite& request_metadata,
       optimization_guide::
           OptimizationGuideModelExecutionResultStreamingCallback callback));
};

// A wrapper that passes through calls to the underlying MockSession. Allows for
// easily mocking calls with a single session object.
class MockSessionWrapper
    : public optimization_guide::OptimizationGuideModelExecutor::Session {
 public:
  explicit MockSessionWrapper(MockSession& session) : session_(session) {}

  void AddContext(
      const google::protobuf::MessageLite& request_metadata) override {
    session_->AddContext(request_metadata);
  }
  void ExecuteModel(
      const google::protobuf::MessageLite& request_metadata,
      optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
          callback) override {
    session_->ExecuteModel(request_metadata, std::move(callback));
  }

 private:
  raw_ref<MockSession> session_;
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
    SetPrefsForComposeConsentState(compose::mojom::ConsentState::kConsented);
    AddTab(browser(), GetPageUrl());
    client_ = ChromeComposeClient::FromWebContents(web_contents());
    client_->SetModelExecutorForTest(&model_executor_);
    client_->SetSkipShowDialogForTest(true);
    client_->SetModelQualityLogsUploaderForTest(&model_quality_logs_uploader_);
    client_->SetSessionIdForTest(base::Token(kSessionIdHigh, kSessionIdLow));

    ON_CALL(model_executor_, StartSession(_)).WillByDefault([&] {
      return std::make_unique<MockSessionWrapper>(session());
    });
    ON_CALL(session(), ExecuteModel(_, _))
        .WillByDefault(testing::WithArg<1>(testing::Invoke(
            [&](optimization_guide::
                    OptimizationGuideModelExecutionResultStreamingCallback
                        callback) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      std::move(callback),
                      OptimizationGuideResponse(
                          ComposeResponse(true, "Cucumbers")),
                      std::make_unique<
                          optimization_guide::ModelQualityLogEntry>(
                          std::make_unique<
                              optimization_guide::proto::LogAiDataRequest>())));
            })));
    test_timer_ = std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  void SetPrefsForComposeConsentState(
      compose::mojom::ConsentState consent_state) {
    PrefService* prefs = GetProfile()->GetPrefs();
    prefs->SetBoolean(prefs::kPrefHasAcceptedComposeConsent, false);
    prefs->SetBoolean(unified_consent::prefs::kPageContentCollectionEnabled,
                      false);
    if (consent_state != compose::mojom::ConsentState::kUnset) {
      prefs->SetBoolean(unified_consent::prefs::kPageContentCollectionEnabled,
                        true);
    }
    if (consent_state == compose::mojom::ConsentState::kConsented) {
      prefs->SetBoolean(prefs::kPrefHasAcceptedComposeConsent, true);
    }
  }

  void ShowDialogAndBindMojo(ComposeCallback callback = base::NullCallback()) {
    ShowDialogAndBindMojoWithFieldData(field_data_, std::move(callback));
  }

  void ShowDialogAndBindMojoWithFieldData(
      autofill::FormFieldData field_data,
      ComposeCallback callback = base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint entry_point =
          autofill::AutofillComposeDelegate::UiEntryPoint::kContextMenu) {
    client().ShowComposeDialog(entry_point, field_data, std::nullopt,
                               std::move(callback));

    BindMojo();
  }

  void BindMojo() {
    client_page_handler_.reset();
    page_handler_.reset();
    // Setup Dialog Page Handler.
    mojo::PendingReceiver<compose::mojom::ComposeClientPageHandler>
        client_page_handler_pending_receiver =
            client_page_handler_.BindNewPipeAndPassReceiver();
    mojo::PendingReceiver<compose::mojom::ComposeSessionPageHandler>
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
    client_->BindComposeDialog(std::move(client_page_handler_pending_receiver),
                               std::move(page_handler_pending_receiver),
                               std::move(callback_router_pending_remote));
  }

  ChromeComposeClient& client() { return *client_; }
  MockSession& session() { return session_; }
  MockModelQualityLogsUploader& model_quality_logs_uploader() {
    return model_quality_logs_uploader_;
  }

  MockComposeDialog& compose_dialog() { return compose_dialog_; }
  autofill::FormFieldData& field_data() { return field_data_; }

  // Get the WebContents for the first browser tab.
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  mojo::Remote<compose::mojom::ComposeClientPageHandler>&
  client_page_handler() {
    return client_page_handler_;
  }

  mojo::Remote<compose::mojom::ComposeSessionPageHandler>& page_handler() {
    return page_handler_;
  }

  GURL GetPageUrl() { return GURL("http://foo/1"); }

  void TearDown() override {
    client_ = nullptr;
    compose::ResetConfigForTesting();
    BrowserWithTestWindowTest::TearDown();
  }

  void SetSelection(const std::u16string& selection) {
    field_data().selected_text = selection;
  }

 protected:
  optimization_guide::proto::ComposePageMetadata ComposePageMetadata() {
    optimization_guide::proto::ComposePageMetadata page_metadata;
    page_metadata.set_page_url(GetPageUrl().spec());
    page_metadata.set_page_title(base::UTF16ToUTF8(
        browser()->tab_strip_model()->GetWebContentsAt(0)->GetTitle()));
    return page_metadata;
  }

  optimization_guide::proto::ComposeRequest ComposeRequest(
      std::string user_input) {
    optimization_guide::proto::ComposeRequest request;
    request.mutable_generate_params()->set_user_input(user_input);
    return request;
  }

  optimization_guide::proto::ComposeResponse ComposeResponse(
      bool ok,
      std::string output) {
    optimization_guide::proto::ComposeResponse response;
    response.set_output(ok ? output : "");
    return response;
  }

  optimization_guide::StreamingResponse OptimizationGuideResponse(
      const optimization_guide::proto::ComposeResponse compose_response,
      bool is_complete = true) {
    optimization_guide::proto::Any any;
    any.set_type_url(kTypeURL);
    compose_response.SerializeToString(any.mutable_value());
    return optimization_guide::StreamingResponse{
        .response = any,
        .is_complete = is_complete,
    };
  }

  const base::HistogramTester& histograms() const { return histogram_tester_; }

  // This helper function is a shortcut to adding a test future to listen for
  // compose responses.
  void BindComposeFutureToOnResponseReceived(
      base::test::TestFuture<compose::mojom::ComposeResponsePtr>&
          compose_future) {
    ON_CALL(compose_dialog(), ResponseReceived(_))
        .WillByDefault(
            testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
              compose_future.SetValue(std::move(response));
            }));
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<ChromeComposeClient> client_;
  testing::NiceMock<MockModelQualityLogsUploader> model_quality_logs_uploader_;
  testing::NiceMock<MockModelExecutor> model_executor_;
  testing::NiceMock<MockSession> session_;
  testing::NiceMock<MockComposeDialog> compose_dialog_;
  autofill::FormFieldData field_data_;
  raw_ptr<content::WebContents> contents_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<mojo::Receiver<compose::mojom::ComposeDialog>>
      callback_router_;
  mojo::Remote<compose::mojom::ComposeClientPageHandler> client_page_handler_;
  mojo::Remote<compose::mojom::ComposeSessionPageHandler> page_handler_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> test_timer_;
};

TEST_F(ChromeComposeClientTest, TestCompose) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")),
                nullptr);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();

  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);

  // Check that a response result OK metric was emitted.
  histograms().ExpectUniqueSample(compose::kComposeResponseStatus,
                                  compose::mojom::ComposeStatus::kOk, 1);
  // Check that a response duration OK metric was emitted.
  histograms().ExpectTotalCount(compose::kComposeResponseDurationOk, 1);
  // Check that a no response duration Error metric was emitted.
  histograms().ExpectTotalCount(compose::kComposeResponseDurationError, 0);
}

TEST_F(ChromeComposeClientTest, TestComposeWithIncompleteResponses) {
  base::test::ScopedFeatureList scoped_feature_list(
      optimization_guide::features::kOptimizationGuideOnDeviceModel);
  base::HistogramTester histogram_tester;

  const std::string input = "a user typed this";
  optimization_guide::proto::ComposeRequest context_request;
  *context_request.mutable_page_metadata() = ComposePageMetadata();
  EXPECT_CALL(session(), AddContext(EqualsProto(context_request)));
  EXPECT_CALL(session(), ExecuteModel(EqualsProto(ComposeRequest(input)), _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            // Start with a partial response.
            callback.Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucu"),
                                          /*is_complete=*/false),
                nullptr);
            // Then send the full response.
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               OptimizationGuideResponse(
                                   ComposeResponse(true, "Cucumbers")),
                               nullptr));
          })));
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose(input, false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucu", result->result);

  result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);

  // Check that a single response result OK metric was emitted.
  histogram_tester.ExpectUniqueSample(compose::kComposeResponseStatus,
                                      compose::mojom::ComposeStatus::kOk, 1);
  // Check that a single response duration OK metric was emitted.
  histogram_tester.ExpectTotalCount(compose::kComposeResponseDurationOk, 1);
  // Check that no response duration Error metric was emitted.
  histogram_tester.ExpectTotalCount(compose::kComposeResponseDurationError, 0);
}

TEST_F(ChromeComposeClientTest, TestComposeSessionIgnoresPreviousResponse) {
  base::test::ScopedFeatureList scoped_feature_list(
      optimization_guide::features::kOptimizationGuideOnDeviceModel);
  base::HistogramTester histogram_tester;

  const std::string input = "a user typed this";
  const std::string input2 = "another input";
  optimization_guide::proto::ComposeRequest context_request;
  *context_request.mutable_page_metadata() = ComposePageMetadata();
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      original_callback;
  EXPECT_CALL(session(), AddContext(EqualsProto(context_request)));
  EXPECT_CALL(session(), ExecuteModel(EqualsProto(ComposeRequest(input)), _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            // Save the callback to call later.
            original_callback = callback;
            // Start with a partial response.
            callback.Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucu"),
                                          /*is_complete=*/false),
                nullptr);
          })));
  EXPECT_CALL(session(), ExecuteModel(EqualsProto(ComposeRequest(input2)), _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            // First call the original callback. This should be ignored.
            original_callback.Run(
                OptimizationGuideResponse(ComposeResponse(true, "old")),
                nullptr);
            // Start with a partial response.
            callback.Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")),
                nullptr);
          })));
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose(input, false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucu", result->result);

  page_handler()->Compose(input2, false);
  result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);

  // Check that a single response result OK metric was emitted.
  histogram_tester.ExpectUniqueSample(compose::kComposeResponseStatus,
                                      compose::mojom::ComposeStatus::kOk, 1);
  // Check that a single response duration OK metric was emitted.
  histogram_tester.ExpectTotalCount(compose::kComposeResponseDurationOk, 1);
  // Check that no response duration Error metric was emitted.
  histogram_tester.ExpectTotalCount(compose::kComposeResponseDurationError, 0);
}

TEST_F(ChromeComposeClientTest, TestComposeParams) {
  ShowDialogAndBindMojo();
  std::string user_input = "a user typed this";
  auto matcher = EqualsProto(ComposeRequest(user_input));
  EXPECT_CALL(session(), ExecuteModel(matcher, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")),
                nullptr);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose(user_input, false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
}

TEST_F(ChromeComposeClientTest, TestComposeNoResponse) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                base::unexpected(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        FromModelExecutionError(
                            optimization_guide::
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                nullptr);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kTryAgainLater, result->status);
}

// Tests that we return an error if Optimization Guide is unable to parse the
// response. In this case the response will be absl::nullopt.
TEST_F(ChromeComposeClientTest, TestComposeNoParsedAny) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(optimization_guide::StreamingResponse(),
                                    nullptr);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kTryAgain, result->status);

  // Check that a response result Try-Again metric was emitted.
  histograms().ExpectUniqueSample(compose::kComposeResponseStatus,
                                  compose::mojom::ComposeStatus::kTryAgain, 1);
  // Check that a response duration Error metric was emitted.
  histograms().ExpectTotalCount(compose::kComposeResponseDurationError, 1);
  // Check that a no response duration OK metric was emitted.
  histograms().ExpectTotalCount(compose::kComposeResponseDurationOk, 0);
}

TEST_F(ChromeComposeClientTest, TestOptimizationGuideDisabled) {
  scoped_feature_list_.Reset();

  // Enable Compose and disable optimization guide model execution.
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {optimization_guide::features::kOptimizationGuideModelExecution});

  ShowDialogAndBindMojo();

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kMisconfiguration, result->status);
}

TEST_F(ChromeComposeClientTest, TestNoModelExecutor) {
  client().SetModelExecutorForTest(nullptr);
  ShowDialogAndBindMojo();

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kMisconfiguration, result->status);
}

TEST_F(ChromeComposeClientTest, TestRestoreStateAfterRequestResponse) {
  ShowDialogAndBindMojo();

  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")),
                nullptr);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

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
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")),
                nullptr);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr>
      compose_test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            compose_test_future.SetValue(std::move(response));
          }));

  page_handler()->SaveWebUIState("web ui state");
  page_handler()->Compose("", false);

  compose::mojom::ComposeResponsePtr response = compose_test_future.Take();
  EXPECT_FALSE(response->undo_available)
      << "First Compose() response should say undo not available.";

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> test_future;
  page_handler()->RequestInitialState(test_future.GetCallback());
  compose::mojom::OpenMetadataPtr open_metadata = test_future.Take();
  EXPECT_EQ("web ui state", open_metadata->compose_state->webui_state);
}

TEST_F(ChromeComposeClientTest, NoStateWorksAtChromeCompose) {
  NavigateAndCommitActiveTab(GURL("chrome://compose"));
  // We skip the dialog showing here, as there is no dialog required at this
  // URL.
  BindMojo();

  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")),
                nullptr);
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();

  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);
}

// Tests that closing after showing the dialog does not crash the browser.
TEST_F(ChromeComposeClientTest, TestCloseUI) {
  ShowDialogAndBindMojo();
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
}

// Tests that closing the session at chrome://compose does not crash the
// browser, even though there is no dialog shown at that URL.
TEST_F(ChromeComposeClientTest, TestCloseUIAtChromeCompose) {
  NavigateAndCommitActiveTab(GURL("chrome://compose"));
  // We skip the dialog showing here, as there is no dialog required at this
  // URL.
  BindMojo();
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
}

// Tests that opening the dialog with user selected text will return that text
// when the WebUI requests initial state.
TEST_F(ChromeComposeClientTest, TestOpenDialogWithSelectedText) {
  field_data().value = u"user selected text";
  SetSelection(u"selected text");
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("selected text", result->initial_input);
}

// Tests that opening the dialog with selected text clears existing state.
TEST_F(ChromeComposeClientTest, TestClearStateWhenOpenWithSelectedText) {
  ShowDialogAndBindMojo();
  page_handler()->SaveWebUIState("web ui state");

  field_data().value = u"user selected text";
  SetSelection(u"selected text");
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("", result->compose_state->webui_state);
  histograms().ExpectBucketCount(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kNewSessionWithSelectedText, 1);
}

TEST_F(ChromeComposeClientTest, TestInputParams) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.input_min_words = 5;
  config.input_max_words = 20;
  config.input_max_chars = 100;
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ(5, result->configurable_params->min_word_limit);
  EXPECT_EQ(20, result->configurable_params->max_word_limit);
  EXPECT_EQ(100, result->configurable_params->max_character_limit);
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
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  page_handler()->Compose("", false);
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

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  page_handler()->SaveWebUIState("this state should be restored with undo");
  page_handler()->Compose("", false);

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";
  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose("", false);

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

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  page_handler()->SaveWebUIState("first state");
  page_handler()->Compose("", false);

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";
  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose("", false);
  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Second Compose() response should "
                                           "say undo is available.";

  page_handler()->SaveWebUIState("third state");
  page_handler()->Compose("", false);

  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Third Compose() response should "
                                           "say undo is available.";

  page_handler()->SaveWebUIState("fourth state");

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

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  page_handler()->SaveWebUIState("first state");
  page_handler()->Compose("", false);

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";

  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose("", false);

  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Second Compose() response should "
                                           "say undo is available.";
  page_handler()->SaveWebUIState("wip web ui state");

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  EXPECT_EQ("first state", undo_future.Take()->webui_state);

  page_handler()->SaveWebUIState("third state");
  page_handler()->Compose("", false);

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

  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideResponse(ComposeResponse(true, "Cucumbers")),
                nullptr);
          })));
  EXPECT_CALL(compose_dialog(), ResponseReceived(_));

  // Before Compose is called AcceptComposeResult will return false.
  base::test::TestFuture<bool> accept_future_1;
  page_handler()->AcceptComposeResult(accept_future_1.GetCallback());
  EXPECT_EQ(false, accept_future_1.Take());

  page_handler()->Compose("a user typed this", false);

  base::test::TestFuture<bool> accept_future_2;
  page_handler()->AcceptComposeResult(accept_future_2.GetCallback());
  EXPECT_EQ(true, accept_future_2.Take());

  // Check that the original callback from Autofill was called correctly.
  EXPECT_EQ(u"Cucumbers", accept_callback.Take());
}

TEST_F(ChromeComposeClientTest, BugReportOpensCorrectURL) {
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

TEST_F(ChromeComposeClientTest, SurveyLinkOpensCorrectURL) {
  GURL survey_url("https://goto.google.com/ccfsfd");

  ShowDialogAndBindMojo();

  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());
  page_handler()->OpenFeedbackSurveyLink();

  // Wait for the resulting new tab to be created.
  tab_add_waiter.Wait();
  // Check that the new foreground tab is opened.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // Check expected URL of the new tab.
  content::WebContents* new_tab_webcontents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(survey_url, new_tab_webcontents->GetVisibleURL());
}

TEST_F(ChromeComposeClientTest, ResetClientOnNavigation) {
  ShowDialogAndBindMojo();

  page_handler()->SaveWebUIState("first state");
  page_handler()->Compose("", false);

  autofill::FormFieldData field_2;
  field_2.unique_renderer_id = autofill::FieldRendererId(2);
  ShowDialogAndBindMojoWithFieldData(field_2);

  // There should be two sessions.
  EXPECT_EQ(2, client().GetSessionCountForTest());

  // Navigate to a new page.
  GURL next_page("http://example.com/a.html");
  NavigateAndCommit(web_contents(), next_page);

  // All session should be deleted.
  EXPECT_EQ(0, client().GetSessionCountForTest());
}

TEST_F(ChromeComposeClientTest, CloseButtonHistogramTest) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  // Simulate three compose request.
  page_handler()->Compose("", false);
  compose::mojom::ComposeResponsePtr response = compose_future.Take();

  page_handler()->Compose("", false);
  response = compose_future.Take();

  page_handler()->Compose("", false);
  response = compose_future.Take();

  // Show the dialog a second time.
  ShowDialogAndBindMojo();

  // Simulate two undos.
  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();
  page_handler()->Undo(undo_future.GetCallback());
  state = undo_future.Take();

  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  histograms().ExpectBucketCount(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCloseButtonPressed, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionComposeCount + std::string(".Ignored"),
      3,  // Expect that three Compose calls were recorded.
      1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionUndoCount + std::string(".Ignored"),
      2,  // Expect that two undos were done.
      1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"),
      2,  // Expect that the dialog was shown twice.
      1);
}

TEST_F(ChromeComposeClientTest, ConsentUICloseReasonHistogramTest) {
  // Set unset consent state and show the dialog
  SetPrefsForComposeConsentState(compose::mojom::ConsentState::kUnset);
  ShowDialogAndBindMojo();

  // Closing the dialog from the consent UI should not log metrics.
  // TODO(b/312295685): Add metrics for consent dialog related close reasons.
  client().CloseUI(compose::mojom::CloseReason::kConsentCloseButton);
  histograms().ExpectTotalCount(compose::kComposeSessionCloseReason, 0);
  histograms().ExpectTotalCount(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 0);

  // Show the dialog a second time.
  ShowDialogAndBindMojo();

  client().CloseUI(compose::mojom::CloseReason::kPageContentConsentDeclined);
  histograms().ExpectTotalCount(compose::kComposeSessionCloseReason, 0);
  histograms().ExpectTotalCount(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 0);
}

TEST_F(ChromeComposeClientTest, ConsentUpdatedHistogramTest) {
  // Set unset consent state and show the dialog
  SetPrefsForComposeConsentState(compose::mojom::ConsentState::kUnset);
  ShowDialogAndBindMojo();

  // If consent is given in this session, then session metrics should be logged.
  client().UpdateAllSessionsWithConsentApproved();
  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  histograms().ExpectBucketCount(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCloseButtonPressed, 1);
}

TEST_F(ChromeComposeClientTest, AcceptSuggestionHistogramTest) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  // Simulate three compose request.
  page_handler()->Compose("", false);
  compose::mojom::ComposeResponsePtr response = compose_future.Take();

  page_handler()->Compose("", false);
  response = compose_future.Take();

  page_handler()->Compose("", false);
  response = compose_future.Take();

  // Show the dialog a second time.
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();

  // Show the dialog a third time.
  ShowDialogAndBindMojo();

  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  histograms().ExpectBucketCount(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kAcceptedSuggestion, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionComposeCount + std::string(".Accepted"),
      3,  // Expect that three Compose calls were recorded.
      1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionUndoCount + std::string(".Accepted"),
      1,  // Expect that one undo was done.
      1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionDialogShownCount + std::string(".Accepted"),
      3,  // Expect that the dialog was shown twice.
      1);
}

TEST_F(ChromeComposeClientTest, LoseFocusHistogramTest) {
  ShowDialogAndBindMojo();

  // Dismiss dialog by losing focus by navigating.
  GURL next_page("http://example.com/a.html");
  NavigateAndCommit(web_contents(), next_page);

  histograms().ExpectBucketCount(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kEndedImplicitly, 1);
}

TEST_F(ChromeComposeClientTest, TestAutoCompose) {
  base::test::TestFuture<void> execute_model_future;
  // Make model execution hang
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(base::test::RunOnceClosure(execute_model_future.GetCallback()));

  std::u16string selected_text = u"ŧëśŧĩňĝ âľpħâ ƅřâɤō ĉħâŗľĩë";
  std::string selected_text_utf8 = base::UTF16ToUTF8(selected_text);
  SetSelection(selected_text);
  ShowDialogAndBindMojo();

  // Check that the UTF8 byte length has zero counts.
  histograms().ExpectBucketCount(compose::kComposeDialogSelectionLength,
                                 base::UTF16ToUTF8(selected_text).size(), 0);
  // Check that the number of UTF8 code points has one count.
  histograms().ExpectBucketCount(
      compose::kComposeDialogSelectionLength,
      base::CountUnicodeCharacters(selected_text_utf8).value(), 1);

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_TRUE(result->compose_state->has_pending_request);

  EXPECT_TRUE(execute_model_future.Wait());
}

TEST_F(ChromeComposeClientTest, TestAutoComposeTooLong) {
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);

  std::u16string words(compose::GetComposeConfig().input_max_chars - 3, u'a');
  words += u" b c";
  SetSelection(words);
  ShowDialogAndBindMojo();

  histograms().ExpectBucketCount(compose::kComposeDialogSelectionLength,
                                 base::UTF16ToUTF8(words).size(), 1);

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

TEST_F(ChromeComposeClientTest, TestAutoComposeTooFewWords) {
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);
  std::u16string words(40, u'a');
  words += u" b";
  SetSelection(words);
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

TEST_F(ChromeComposeClientTest, TestAutoComposeTooManyWords) {
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);

  std::u16string words = u"b";
  // Words should be the max plus 1.
  for (uint32_t i = 0; i < compose::GetComposeConfig().input_max_words; ++i) {
    words += u" b";
  }
  SetSelection(words);
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

TEST_F(ChromeComposeClientTest, TestAutoComposeDisabled) {
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);

  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{compose::features::kEnableCompose,
                             {{"auto_submit_with_selection", "false"}}},
                            {optimization_guide::features::
                                 kOptimizationGuideModelExecution,
                             {}}},
      /*disabled_features=*/{});
  // Needed for feature flags to apply.
  compose::ResetConfigForTesting();

  SetSelection(u"testing alpha bravo charlie");
  ShowDialogAndBindMojo();
}

TEST_F(ChromeComposeClientTest, TestNoAutoComposeWithPopup) {
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);
  SetSelection(u"a");  // too short to cause auto compose.

  ShowDialogAndBindMojo();

  SetSelection(u"testing alpha bravo charlie");

  // Show again.
  ShowDialogAndBindMojoWithFieldData(
      field_data(), base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

TEST_F(ChromeComposeClientTest, TestAutoComposeWithRepeatedRightClick) {
  base::test::TestFuture<void> execute_model_future;
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(base::test::RunOnceClosure(execute_model_future.GetCallback()));

  SetSelection(u"a");  // too short to cause auto compose.

  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_FALSE(result->compose_state->has_pending_request);

  std::u16string selection = u"testing alpha bravo charlie";
  SetSelection(selection);

  // Show again.
  ShowDialogAndBindMojo();

  EXPECT_TRUE(execute_model_future.Wait());

  page_handler()->RequestInitialState(open_test_future.GetCallback());
  result = open_test_future.Take();
  EXPECT_TRUE(result->compose_state->has_pending_request);
  EXPECT_EQ(base::UTF16ToUTF8(selection), result->initial_input);
}

TEST_F(ChromeComposeClientTest, TestNoAutoComposeWithoutConsent) {
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);

  SetPrefsForComposeConsentState(compose::mojom::ConsentState::kUnset);
  // Valid selection for auto compose to use.
  std::u16string selection = u"testing alpha bravo charlie";
  SetSelection(selection);
  ShowDialogAndBindMojo();

  // Without consent auto compose should not execute.
  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

TEST_F(ChromeComposeClientTest, TestComposeQualitySessionId) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(2);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

  EXPECT_TRUE(compose_future.Wait());
  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed that", false);
  EXPECT_TRUE(compose_future.Wait());

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();
  EXPECT_TRUE(state)
      << "Undo should return valid state after second Compose() invocation.";

  // This take should clear the test future for the second commit.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();

  EXPECT_EQ(kSessionIdHigh,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->session_id()
                .high());

  EXPECT_EQ(kSessionIdLow,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->session_id()
                .low());

  // Close UI to submit quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  result = quality_test_future.Take();

  EXPECT_EQ(kSessionIdHigh,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->session_id()
                .high());
  EXPECT_EQ(kSessionIdLow,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->session_id()
                .low());
}

TEST_F(ChromeComposeClientTest, TestComposeQualityLatency) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(2);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

  EXPECT_TRUE(compose_future.Wait());
  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed that", false);

  // Ensure compose is finished before calling undo
  EXPECT_TRUE(compose_future.Wait());

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();
  EXPECT_TRUE(state)
      << "Undo should return valid state after second Compose() invocation.";

  // This take should clear the quality future from the model that was undone.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();

  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->request_latency_ms());

  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  result = quality_test_future.Take();

  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->request_latency_ms());
}

TEST_F(ChromeComposeClientTest,
       TestComposeQualityOnlyOneLogEntryAbandonedOnClose) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(2);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future_2;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            if (!quality_test_future.IsReady()) {
              quality_test_future.SetValue(std::move(response));
            } else {
              quality_test_future_2.SetValue(std::move(response));
            }
          }));

  page_handler()->Compose("a user typed this", false);

  EXPECT_TRUE(compose_future.Wait());  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed that", false);

  EXPECT_TRUE(compose_future.Wait());
  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  // This take should clear the quality future from the model that was undone.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();

  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_ABANDONED,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->final_status());

  result = quality_test_future_2.Take();

  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_UNSPECIFIED,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->final_status());
}

TEST_F(ChromeComposeClientTest, TestComposeQualityWasEdited) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(2);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future_2;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            if (!quality_test_future.IsReady()) {
              quality_test_future.SetValue(std::move(response));
            } else {
              quality_test_future_2.SetValue(std::move(response));
            }
          }));

  page_handler()->Compose("a user typed this", false);

  EXPECT_TRUE(compose_future.Wait());  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed that", true);

  EXPECT_TRUE(compose_future.Wait());
  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  // This take should clear the quality future from the model that was undone.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();

  EXPECT_TRUE(result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                  ->was_generated_via_edit());

  result = quality_test_future_2.Take();

  EXPECT_FALSE(result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                   ->was_generated_via_edit());
}

#if defined(GTEST_HAS_DEATH_TEST)
// Tests that the Compose client crashes the browser if a webcontents
// tries to bind mojo without opening the dialog at a non Compose URL.
TEST_F(ChromeComposeClientTest, NoStateCrashesAtOtherUrls) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  // We skip the dialog showing here, to validate that non special URLs check.
  EXPECT_DEATH(BindMojo(), "");
}

// Tests that the Compose client crashes the browser if a webcontents
// sends any message when the dialog has not been shown.
TEST_F(ChromeComposeClientTest, TestCannotSendMessagesToNotShownDialog) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(page_handler()->SaveWebUIState(""), "");
}

// Tests that the Compose client crashes the browser if a webcontents
// tries to close the dialog when the dialog has not been shown.
TEST_F(ChromeComposeClientTest, TestCannotCloseNotShownDialog) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(
      client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton),
      "");
}

// Tests that the Compose client crashes the browser if a webcontents
// tries to close the dialog when the dialog has not been shown.
TEST_F(ChromeComposeClientTest, TestCannotSendMessagesAfterClosingDialog) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ShowDialogAndBindMojo();
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
  // Any message after closing the session will crash.
  EXPECT_DEATH(page_handler()->SaveWebUIState(""), "");
}

// Tests that the Compose client crashes the browser if a webcontents
// sends any more messages after closing the dialog at chrome://contents.
TEST_F(ChromeComposeClientTest,
       TestCannotSendMessagesAfterClosingDialogAtChromeCompose) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  NavigateAndCommitActiveTab(GURL("chrome://compose"));
  // We skip the dialog showing here, as there is no dialog required at this
  // URL.
  BindMojo();
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
  // Any message after closing the session will crash.
  EXPECT_DEATH(page_handler()->SaveWebUIState(""), "");
}
#endif  // GTEST_HAS_DEATH_TEST
