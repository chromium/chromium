// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/common/compose/compose.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using base::test::RunOnceCallback;
using testing::_;
using ComposeCallback = base::OnceCallback<void(const std::u16string&)>;
using optimization_guide::OptimizationGuideModelExecutionError;
using optimization_guide::
    OptimizationGuideModelExecutionResultStreamingCallback;
using optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using optimization_guide::StreamingResponse;
using segmentation_platform::MockSegmentationPlatformService;

namespace {

const uint64_t kSessionIdHigh = 1234;
const uint64_t kSessionIdLow = 5678;
const segmentation_platform::TrainingRequestId kTrainingRequestId =
    segmentation_platform::TrainingRequestId(456);
constexpr char kTypeURL[] =
    "type.googleapis.com/optimization_guide.proto.ComposeResponse";

class MockInnerText : public InnerTextProvider {
 public:
  MOCK_METHOD(void,
              GetInnerText,
              (content::RenderFrameHost & host,
               std::optional<int> node_id,
               content_extraction::InnerTextCallback callback));
};

class MockModelExecutor
    : public optimization_guide::OptimizationGuideModelExecutor {
 public:
  MOCK_METHOD(bool,
              CanCreateOnDeviceSession,
              (optimization_guide::ModelBasedCapabilityKey feature,
               raw_ptr<optimization_guide::OnDeviceModelEligibilityReason>
                   debug_reason));
  MOCK_METHOD(std::unique_ptr<Session>,
              StartSession,
              (optimization_guide::ModelBasedCapabilityKey feature,
               const std::optional<optimization_guide::SessionConfigParams>&
                   config_params));
  MOCK_METHOD(void,
              ExecuteModel,
              (optimization_guide::ModelBasedCapabilityKey feature,
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

class MockComposeDialog : public compose::mojom::ComposeUntrustedDialog {
 public:
  MOCK_METHOD(void,
              ResponseReceived,
              (compose::mojom::ComposeResponsePtr response));
  MOCK_METHOD(void,
              PartialResponseReceived,
              (compose::mojom::PartialComposeResponsePtr response));
};

}  // namespace

class ChromeComposeClientTest : public BrowserWithTestWindowTest {
 public:
  ChromeComposeClientTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    scoped_compose_enabled_ = ComposeEnabling::ScopedEnableComposeForTesting();
    BrowserWithTestWindowTest::SetUp();

    segmentation_platform::SegmentationPlatformServiceFactory::GetInstance()
        ->SetTestingFactory(
            GetProfile(),
            base::BindLambdaForTesting([](content::BrowserContext* context) {
              std::unique_ptr<KeyedService> result =
                  std::make_unique<MockSegmentationPlatformService>();
              return result;
            }));

    scoped_feature_list_.InitWithFeatures(
        {compose::features::kEnableCompose,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
    // Needed for feature params to reset.
    compose::ResetConfigForTesting();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                         true);
    SetPrefsForComposeMSBBState(true);
    AddTab(browser(), GetPageUrl());
    client_ = ChromeComposeClient::FromWebContents(web_contents());
    client_->SetModelExecutorForTest(&model_executor_);
    client_->SetInnerTextProviderForTest(&model_inner_text_);
    client_->SetSkipShowDialogForTest(true);
    client_->SetModelQualityLogsUploaderForTest(&model_quality_logs_uploader_);
    client_->SetSessionIdForTest(base::Token(kSessionIdHigh, kSessionIdLow));

    ON_CALL(model_inner_text(), GetInnerText(_, _, _))
        .WillByDefault(testing::WithArg<2>(testing::Invoke(
            [&](content_extraction::InnerTextCallback callback) {
              std::unique_ptr<content_extraction::InnerTextResult>
                  expected_inner_text =
                      std::make_unique<content_extraction::InnerTextResult>("",
                                                                            0);
              std::move(callback).Run(std::move(expected_inner_text));
            })));
    ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
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
                      OptimizationGuideModelStreamingExecutionResult(
                          base::ok(OptimizationGuideResponse(
                              ComposeResponse(true, "Cucumbers"))),
                          /*provided_by_on_device=*/false,
                          std::make_unique<
                              optimization_guide::ModelQualityLogEntry>(
                              std::make_unique<optimization_guide::proto::
                                                   LogAiDataRequest>(),
                              nullptr))));
            })));

    ON_CALL(GetSegmentationPlatformService(),
            GetClassificationResult(_, _, _, _))
        .WillByDefault(testing::WithArg<3>(testing::Invoke(
            [](segmentation_platform::ClassificationResultCallback callback) {
              auto result = segmentation_platform::ClassificationResult(
                  segmentation_platform::PredictionStatus::kSucceeded);
              result.request_id = kTrainingRequestId;
              result.ordered_labels = {
                  segmentation_platform::kComposePrmotionLabelShow};
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), result));
            })));

    test_timer_ = std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  void TearDown() override {
    // Clear default actions for safe teardown.
    testing::Mock::VerifyAndClear(&GetSegmentationPlatformService());
    client_ = nullptr;
    scoped_feature_list_.Reset();
    ukm_recorder_.reset();
    // Needed for feature params to reset.
    compose::ResetConfigForTesting();
    BrowserWithTestWindowTest::TearDown();
  }

  void SetPrefsForComposeMSBBState(bool msbb_state) {
    PrefService* prefs = GetProfile()->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        msbb_state);
  }

  void EnableAutoCompose() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{compose::features::kEnableCompose,
                              optimization_guide::features::
                                  kOptimizationGuideModelExecution,
                              compose::features::kComposeAutoSubmit},
        /*disabled_features=*/{});
    // Needed for feature params to apply.
    compose::ResetConfigForTesting();
  }

  void ShowDialogAndBindMojo(ComposeCallback callback = base::NullCallback()) {
    ShowDialogAndBindMojoWithFieldData(field_data(), std::move(callback));
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
    mojo::PendingReceiver<compose::mojom::ComposeClientUntrustedPageHandler>
        client_page_handler_pending_receiver =
            client_page_handler_.BindNewPipeAndPassReceiver();
    mojo::PendingReceiver<compose::mojom::ComposeSessionUntrustedPageHandler>
        page_handler_pending_receiver =
            page_handler_.BindNewPipeAndPassReceiver();

    // Setup Compose Dialog.
    callback_router_.reset();
    callback_router_ = std::make_unique<
        mojo::Receiver<compose::mojom::ComposeUntrustedDialog>>(
        &compose_dialog());
    mojo::PendingRemote<compose::mojom::ComposeUntrustedDialog>
        callback_router_pending_remote =
            callback_router_->BindNewPipeAndPassRemote();

    // Bind mojo to client.
    client_->BindComposeDialog(std::move(client_page_handler_pending_receiver),
                               std::move(page_handler_pending_receiver),
                               std::move(callback_router_pending_remote));
  }

  void FlushMojo() {
    client_page_handler().FlushForTesting();
    page_handler().FlushForTesting();
  }

  ChromeComposeClient& client() { return *client_; }
  MockSession& session() { return session_; }
  MockModelQualityLogsUploader& model_quality_logs_uploader() {
    return model_quality_logs_uploader_;
  }
  MockInnerText& model_inner_text() { return model_inner_text_; }

  MockComposeDialog& compose_dialog() { return compose_dialog_; }
  autofill::FormFieldData& field_data() { return field_data_; }

  // Get the WebContents for the first browser tab.
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  mojo::Remote<compose::mojom::ComposeClientUntrustedPageHandler>&
  client_page_handler() {
    return client_page_handler_;
  }

  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return *ukm_recorder_; }

  mojo::Remote<compose::mojom::ComposeSessionUntrustedPageHandler>&
  page_handler() {
    return page_handler_;
  }

  GURL GetPageUrl() { return GURL("http://foo/1"); }

  void SetSelection(const std::u16string& selection) {
    field_data().set_selected_text(selection);
  }

  // Emulate selected text truncation performed by Autofill.
  void SetSelectionWithTruncation(const std::u16string& selection,
                                  size_t max_length) {
    field_data().set_selected_text(selection.substr(0, max_length));
  }

  MockSegmentationPlatformService& GetSegmentationPlatformService() {
    return *static_cast<MockSegmentationPlatformService*>(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetForProfile(GetProfile()));
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

  optimization_guide::proto::ComposeRequest RegenerateRequest(
      std::string previous_response) {
    optimization_guide::proto::ComposeRequest request;
    request.mutable_rewrite_params()->set_regenerate(true);
    request.mutable_rewrite_params()->set_previous_response(previous_response);
    return request;
  }

  optimization_guide::proto::ComposeResponse ComposeResponse(
      bool ok,
      std::string output) {
    optimization_guide::proto::ComposeResponse response;
    response.set_output(ok ? output : "");
    return response;
  }

  StreamingResponse OptimizationGuideResponse(
      const optimization_guide::proto::ComposeResponse compose_response,
      bool is_complete = true) {
    optimization_guide::proto::Any any;
    any.set_type_url(kTypeURL);
    compose_response.SerializeToString(any.mutable_value());
    return StreamingResponse{
        .response = any,
        .is_complete = is_complete,
    };
  }

  OptimizationGuideModelStreamingExecutionResult
  OptimizationGuideStreamingResult(
      const optimization_guide::proto::ComposeResponse compose_response,
      bool is_complete = true,
      bool provided_by_on_device = false,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry =
          nullptr) {
    return OptimizationGuideModelStreamingExecutionResult(
        base::ok(OptimizationGuideResponse(compose_response, is_complete)),
        provided_by_on_device, std::move(log_entry));
  }

  const base::HistogramTester& histograms() const { return histogram_tester_; }

  const base::UserActionTester& user_action_tester() const {
    return user_action_tester_;
  }

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

  autofill::TestBrowserAutofillManager* autofill_manager() {
    return autofill_manager_injector_[web_contents()];
  }

  autofill::TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<ChromeComposeClient> client_;
  testing::NiceMock<MockModelQualityLogsUploader> model_quality_logs_uploader_;
  testing::NiceMock<MockModelExecutor> model_executor_;
  testing::NiceMock<MockInnerText> model_inner_text_;
  testing::NiceMock<MockSession> session_;
  testing::NiceMock<MockComposeDialog> compose_dialog_;
  autofill::FormFieldData field_data_;
  raw_ptr<content::WebContents> contents_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillManagerInjector<autofill::TestBrowserAutofillManager>
      autofill_manager_injector_;

  std::unique_ptr<mojo::Receiver<compose::mojom::ComposeUntrustedDialog>>
      callback_router_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  mojo::Remote<compose::mojom::ComposeClientUntrustedPageHandler>
      client_page_handler_;
  mojo::Remote<compose::mojom::ComposeSessionUntrustedPageHandler>
      page_handler_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> test_timer_;
  ComposeEnabling::ScopedOverride scoped_compose_enabled_;
};

TEST_F(ChromeComposeClientTest, TestCompose) {
  // Simulate page showing context menu.
  auto* rfh =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_content_editable_for_autofill = true;
  params.frame_origin = rfh->GetMainFrame()->GetLastCommittedOrigin();
  EXPECT_TRUE(client().ShouldTriggerContextMenu(rfh, params));

  // Then simulate clicking the dialog.
  ShowDialogAndBindMojo();

  // Now call Compose, checking the results.
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();

  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);
  EXPECT_FALSE(result->on_device_evaluation_used);

  // Check that a user action for the Compose request was emitted.
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Compose.ComposeRequest.CreateClicked"));
  histograms().ExpectUniqueSample(compose::kComposeRequestReason,
                                  compose::ComposeRequestReason::kFirstRequest,
                                  1);
  histograms().ExpectUniqueSample("Compose.Server.Request.Reason",
                                  compose::ComposeRequestReason::kFirstRequest,
                                  1);
  // Check that a request result OK metric was emitted.
  histograms().ExpectUniqueSample(compose::kComposeRequestStatus,
                                  compose::mojom::ComposeStatus::kOk, 1);
  histograms().ExpectUniqueSample("Compose.Server.Request.Status",
                                  compose::mojom::ComposeStatus::kOk, 1);

  // Check that a request duration OK metric was emitted.
  histograms().ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationOkSuffix}), 1);
  histograms().ExpectTotalCount(
      base::StrCat(
          {"Compose.Server", compose::kComposeRequestDurationOkSuffix}),
      1);

  // Check that no request duration Error metrics were emitted.
  histograms().ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationErrorSuffix}),
      0);
  histograms().ExpectTotalCount(
      base::StrCat(
          {"Compose.Server", compose::kComposeRequestDurationErrorSuffix}),
      0);
  // Check that the request metadata had a valid node offset.
  histograms().ExpectUniqueSample(
      compose::kInnerTextNodeOffsetFound,
      compose::ComposeInnerTextNodeOffset::kOffsetFound, 1);
  // Simulate insert call from Compose dialog.
  page_handler()->AcceptComposeResult(base::NullCallback());
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kInsertButton);
  FlushMojo();

  // Check Compose Session Event Counts.
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Session.EventCounts",
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      "Compose.OnDevice.Session.EventCounts",
      compose::ComposeSessionEventTypes::kMainDialogShown, 0);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Session.EventCounts",
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
  histograms().ExpectBucketCount(
      "Compose.OnDevice.Session.EventCounts",
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 0);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kCreateClicked, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kInsertClicked, 1);

  histograms().ExpectUniqueSample("Compose.Session.EvalLocation",
                                  compose::SessionEvalLocation::kServer, 1);

  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check page level UKM metrics.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName});

  EXPECT_EQ(ukm_entries.size(), 1UL);

  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemShownName,
                        1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 1)));

  // Check session level UKM metrics.
  auto session_ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_SessionProgress::kEntryName,
      {ukm::builders::Compose_SessionProgress::kComposeCountName,
       ukm::builders::Compose_SessionProgress::kDialogShownCountName,
       ukm::builders::Compose_SessionProgress::kDialogShownCountName,
       ukm::builders::Compose_SessionProgress::kUndoCountName,
       ukm::builders::Compose_SessionProgress::kRegenerateCountName,
       ukm::builders::Compose_SessionProgress::kShortenCountName,
       ukm::builders::Compose_SessionProgress::kLengthenCountName,
       ukm::builders::Compose_SessionProgress::kFormalCountName,
       ukm::builders::Compose_SessionProgress::kCasualCountName,
       ukm::builders::Compose_SessionProgress::kInsertedResultsName,
       ukm::builders::Compose_SessionProgress::kCanceledName});

  EXPECT_EQ(session_ukm_entries.size(), 1UL);

  EXPECT_THAT(
      session_ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kComposeCountName, 1),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kDialogShownCountName, 1),
          testing::Pair(ukm::builders::Compose_SessionProgress::kUndoCountName,
                        0),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kRegenerateCountName, 0),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kShortenCountName, 0),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kLengthenCountName, 0),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kFormalCountName, 0),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kCasualCountName, 0),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kInsertedResultsName, 1),
          testing::Pair(ukm::builders::Compose_SessionProgress::kCanceledName,
                        0)));
}

TEST_F(ChromeComposeClientTest, TestComposeServerAndOnDeviceResponses) {
  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);
  EXPECT_FALSE(result->on_device_evaluation_used);

  // Simulate rewrite, serviced by on-device model.
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Tomatoes"), true,
                /*provided_by_on_device=*/true));
          })));

  page_handler()->Rewrite(compose::mojom::StyleModifier::kRetry);

  // Simulate insert call from Compose dialog.
  page_handler()->AcceptComposeResult(base::NullCallback());
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kInsertButton);
  FlushMojo();

  histograms().ExpectUniqueSample("Compose.Session.EvalLocation",
                                  compose::SessionEvalLocation::kMixed, 1);

  histograms().ExpectBucketCount(compose::kComposeRequestReason,
                                 compose::ComposeRequestReason::kFirstRequest,
                                 1);
  histograms().ExpectUniqueSample("Compose.Server.Request.Reason",
                                  compose::ComposeRequestReason::kFirstRequest,
                                  1);
  histograms().ExpectBucketCount(compose::kComposeRequestReason,
                                 compose::ComposeRequestReason::kRetryRequest,
                                 1);
  histograms().ExpectUniqueSample("Compose.OnDevice.Request.Reason",
                                  compose::ComposeRequestReason::kRetryRequest,
                                  1);
  // Check that only the location agnostic metrics are recorded.
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Session.EventCounts",
      compose::ComposeSessionEventTypes::kMainDialogShown, 0);
  histograms().ExpectBucketCount(
      "Compose.OnDevice.Session.EventCounts",
      compose::ComposeSessionEventTypes::kMainDialogShown, 0);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Session.EventCounts",
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 0);
  histograms().ExpectBucketCount(
      "Compose.OnDevice.Session.EventCounts",
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 0);
}

TEST_F(ChromeComposeClientTest, TestComposeOnDeviceSessionHistograms) {
  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);

  // Simulate rewrite, serviced by on-device model.
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Tomatoes"), true,
                /*provided_by_on_device=*/true));
          })));

  page_handler()->Compose("", false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Tomatoes", result->result);
  EXPECT_TRUE(result->on_device_evaluation_used);

  // Simulate insert call from Compose dialog.
  page_handler()->AcceptComposeResult(base::NullCallback());
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kInsertButton);
  FlushMojo();

  histograms().ExpectUniqueTimeSample(
      "Compose.OnDevice.Session.Duration.Inserted",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms().ExpectUniqueSample(
      "Compose.OnDevice.Session.DialogShownCount.Accepted", 1, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Session.EventCounts",
      compose::ComposeSessionEventTypes::kMainDialogShown, 0);
  histograms().ExpectBucketCount(
      "Compose.OnDevice.Session.EventCounts",
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Session.EventCounts",
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 0);
  histograms().ExpectBucketCount(
      "Compose.OnDevice.Session.EventCounts",
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
}

TEST_F(ChromeComposeClientTest, TestComposeEmptySession) {
  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kInsertButton);
  FlushMojo();

  histograms().ExpectUniqueSample("Compose.Session.EvalLocation",
                                  compose::SessionEvalLocation::kNone, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
}

TEST_F(ChromeComposeClientTest, TestComposeShowContextMenu) {
  auto* rfh =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_content_editable_for_autofill = true;
  params.frame_origin = rfh->GetMainFrame()->GetLastCommittedOrigin();

  EXPECT_TRUE(client().ShouldTriggerContextMenu(rfh, params));
  NavigateAndCommitActiveTab(GURL("about:blank"));

  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName});

  EXPECT_EQ(ukm_entries.size(), 1UL);

  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemShownName,
                        1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 0)));

  // Now show context menu twice on same page and verify that second UKM record
  // reflects this.
  EXPECT_TRUE(client().ShouldTriggerContextMenu(rfh, params));
  EXPECT_TRUE(client().ShouldTriggerContextMenu(rfh, params));
  NavigateAndCommitActiveTab(GURL("about:blank"));

  ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName});

  EXPECT_EQ(ukm_entries.size(), 2UL);

  EXPECT_THAT(
      ukm_entries[1].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemShownName,
                        2),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 0)));
}

TEST_F(ChromeComposeClientTest, TestComposeShowContextMenuAndDialog) {
  auto* rfh =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_content_editable_for_autofill = true;
  params.frame_origin = rfh->GetMainFrame()->GetLastCommittedOrigin();

  EXPECT_TRUE(client().ShouldTriggerContextMenu(rfh, params));
  ShowDialogAndBindMojo();

  NavigateAndCommitActiveTab(GURL("about:blank"));

  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShownName});

  EXPECT_EQ(ukm_entries.size(), 1UL);

  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemShownName,
                        1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 0),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeShownName, 0)));

  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
}

TEST_F(ChromeComposeClientTest, TestProactiveNudgeEngagementIsRecorded) {
  // Enable and trigger the proactive nudge.
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_show_probability = 1.0;
  config.proactive_nudge_delay = base::Microseconds(1);
  config.proactive_nudge_segmentation = true;
  config.proactive_nudge_always_collect_training_data = true;

  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  autofill::FormFieldData selected_field_data = form_data.fields[0];
  selected_field_data.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange;

  ASSERT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));

  task_environment()->FastForwardBy(config.proactive_nudge_delay);

  ASSERT_TRUE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                          trigger_source));

  // Simulate clicking on the nudge to open compose.
  ShowDialogAndBindMojoWithFieldData(
      selected_field_data, base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);

  base::test::TestFuture<segmentation_platform::TrainingLabels> training_labels;
  EXPECT_CALL(GetSegmentationPlatformService(),
              CollectTrainingData(
                  segmentation_platform::proto::SegmentId::
                      OPTIMIZATION_TARGET_SEGMENTATION_COMPOSE_PROMOTION,
                  kTrainingRequestId, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&](auto labels) { training_labels.SetValue(labels); })));

  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  // Trigger session deletion and verify that the engagement is recorded.
  NavigateAndCommitActiveTab(GURL("about:blank"));
  EXPECT_EQ(training_labels.Get().output_metric,
            std::make_pair("Compose.ProactiveNudge.DerivedEngagement",
                           static_cast<base::HistogramBase::Sample>(
                               compose::ProactiveNudgeDerivedEngagement::
                                   kAcceptedComposeSuggestion)));
}

TEST_F(ChromeComposeClientTest,
       TestShouldTriggerProactiveNudgeBlockedBySegmentation) {
  // Enable and trigger the proactive nudge.
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_show_probability = 1.0;
  config.proactive_nudge_delay = base::Microseconds(1);
  config.proactive_nudge_segmentation = true;

  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  autofill::FormFieldData selected_field_data = form_data.fields[0];
  selected_field_data.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  EXPECT_CALL(GetSegmentationPlatformService(),
              GetClassificationResult(_, _, _, _))
      .WillOnce(testing::WithArg<3>(testing::Invoke(
          [](segmentation_platform::ClassificationResultCallback callback) {
            auto result = segmentation_platform::ClassificationResult(
                segmentation_platform::PredictionStatus::kSucceeded);
            result.request_id = kTrainingRequestId;
            result.ordered_labels = {
                segmentation_platform::kComposePrmotionLabelDontShow};
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), result));
          })));

  // The initial trigger request comes from a text field change.
  EXPECT_FALSE(client().ShouldTriggerPopup(
      form_data, selected_field_data,
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  task_environment()->FastForwardBy(config.proactive_nudge_delay);

  // All remaining popup trigger requests come from the delayed nudge.
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge;

  ASSERT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));
  histograms().ExpectBucketCount(
      compose::kComposeProactiveNudgeShowStatus,
      compose::ComposeShowStatus::kProactiveNudgeBlockedBySegmentationPlatform,
      1);

  // Commit metrics on page navigation.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check that the proactive nudge UKM was still captured.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShownName});

  ASSERT_EQ(ukm_entries.size(), 1UL);

  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemShownName,
                        0),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 0),

          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName,
              1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeShownName, 0)));

  // Check that even after a second call only one show status UMA was recorded.
  EXPECT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));
  histograms().ExpectBucketCount(
      compose::kComposeProactiveNudgeShowStatus,
      compose::ComposeShowStatus::kProactiveNudgeBlockedBySegmentationPlatform,
      1);
}

TEST_F(ChromeComposeClientTest, TestShouldTriggerProactiveNudgeDisabledUKM) {
  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  autofill::FormFieldData selected_field_data = form_data.fields[0];
  selected_field_data.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange;

  // By default the proactive nudge is disabled.
  EXPECT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));

  // Commit metrics on page navigation.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check that the proactive nudge UKM was still captured.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShownName});

  ASSERT_EQ(ukm_entries.size(), 1UL);

  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemShownName,
                        0),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 0),

          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName,
              1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeShownName, 0)));
}

TEST_F(ChromeComposeClientTest, TestShouldTriggerProactiveNudgeEnabled) {
  // Enable proactive nudge.
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_delay = base::Seconds(0);
  config.proactive_nudge_segmentation = false;

  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  autofill::FormFieldData selected_field_data = form_data.fields[0];
  selected_field_data.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange;

  EXPECT_TRUE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                          trigger_source));

  // Commit metrics on page navigation.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check that the proactive nudge UKM was still captured.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShownName});

  ASSERT_EQ(ukm_entries.size(), 1UL);

  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(ukm::builders::Compose_PageEvents::kMenuItemShownName,
                        0),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 0),

          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName,
              1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeShownName, 1)));

  // Check Compose.ProactiveNudge.CTR metrics.
  histograms().ExpectBucketCount(
      compose::kComposeProactiveNudgeCtr,
      compose::ComposeProactiveNudgeCtrEvent::kNudgeDisplayed, 1);
}

TEST_F(ChromeComposeClientTest,
       TestShouldTriggerProactiveNudgePageChecksFailUKM) {
  autofill::FormData form_data;
  form_data.set_url(GURL("www.example.com"));
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  autofill::FormFieldData selected_field_data = form_data.fields[0];
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange;

  // Will fail because field origin does not match page origin.
  EXPECT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));

  // Commit metrics on page navigation.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check that the proactive nudge UKM was not captured.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName});

  ASSERT_EQ(ukm_entries.size(), 0UL);
}

TEST_F(ChromeComposeClientTest, TestProactiveNudgeMSBBDisabled) {
  SetPrefsForComposeMSBBState(false);
  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  autofill::FormFieldData selected_field_data = form_data.fields[0];
  selected_field_data.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange;

  // Will fail because MSBB is not set
  EXPECT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));

  histograms().ExpectBucketCount(
      compose::kComposeProactiveNudgeShowStatus,
      compose::ComposeShowStatus::kProactiveNudgeDisabledByMSBB, 1);
}

TEST_F(ChromeComposeClientTest, TestCaretMovementExtendsNudgeDelay) {
  using Observer = autofill::AutofillManager::Observer;

  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_show_probability = 1.0;
  config.proactive_nudge_delay = base::Microseconds(4);
  config.proactive_nudge_segmentation = false;

  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  autofill::FormFieldData field_data = form_data.fields[0];
  field_data.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  field_data.set_host_frame(form_data.host_frame());
  field_data.set_host_form_id(form_data.renderer_id());

  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillClient::FromWebContents(web_contents())
          ->GetAutofillDriverFactory()
          ->DriverForFrame(web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(autofill_driver);

  {
    autofill::TestAutofillManagerWaiter waiter(
        autofill_driver->GetAutofillManager(),
        {autofill::AutofillManagerEvent::kFormsSeen});
    autofill_driver->renderer_events().FormsSeen(/*updated_forms=*/{form_data},
                                                 /*removed_forms=*/{});
    ASSERT_TRUE(waiter.Wait(/*num_awaiting_calls=*/1));
  }

  // The first call to ShouldTriggerPopup starts the nudge tracker timers.
  ASSERT_FALSE(client().ShouldTriggerPopup(
      form_data, field_data,
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  task_environment()->FastForwardBy(base::Microseconds(3));
  ASSERT_TRUE(client().IsPopupTimerRunning());

  // Should trigger will fail since not enough time has passed.
  ASSERT_FALSE(
      client().ShouldTriggerPopup(form_data, field_data,
                                  autofill::AutofillSuggestionTriggerSource::
                                      kComposeDelayedProactiveNudge));

  // Signal that a the caret moved in the field with no selection.
  autofill_driver->GetAutofillManager().NotifyObservers(
      &Observer::OnAfterCaretMovedInFormField, form_data.global_id(),
      field_data.global_id(), /*selected_text=*/u"",
      /*caret_bounds=*/gfx::Rect());

  // Moving the caret should extend the timer so it is still running.
  task_environment()->FastForwardBy(base::Microseconds(3));
  ASSERT_TRUE(client().IsPopupTimerRunning());

  // Should trigger will fail since not enough time has passed.
  ASSERT_FALSE(
      client().ShouldTriggerPopup(form_data, field_data,
                                  autofill::AutofillSuggestionTriggerSource::
                                      kComposeDelayedProactiveNudge));

  // Move forward until timer should expire.
  task_environment()->FastForwardBy(base::Microseconds(2));
  ASSERT_FALSE(client().IsPopupTimerRunning());

  // Should trigger will now succeed.
  ASSERT_TRUE(
      client().ShouldTriggerPopup(form_data, field_data,
                                  autofill::AutofillSuggestionTriggerSource::
                                      kComposeDelayedProactiveNudge));
}

TEST_F(ChromeComposeClientTest, TestComposeShouldTriggerSavedStateNudgeUKM) {
  autofill::FormData form_data;
  form_data.set_url(GetPageUrl());
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  const autofill::FormFieldData selected_field_data = form_data.fields[0];
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange;

  // Start a Compose session on selected field.
  ShowDialogAndBindMojoWithFieldData(selected_field_data);

  // By default the saved state nudge is shown.
  EXPECT_TRUE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                          trigger_source));

  // Commit metrics on page navigation.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check that no proactive nudge UKM was recorded.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName});

  EXPECT_EQ(ukm_entries.size(), 0UL);
}

TEST_F(ChromeComposeClientTest, TestComposeWithIncompleteResponsesAnimated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationGuideOnDeviceModel,
       compose::features::kComposeTextOutputAnimation},
      {});

  const std::string input = "a user typed this";
  optimization_guide::proto::ComposeRequest context_request;
  *context_request.mutable_page_metadata() = ComposePageMetadata();
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      saved_callback;
  EXPECT_CALL(session(), AddContext(EqualsProto(context_request)));
  EXPECT_CALL(session(), ExecuteModel(EqualsProto(ComposeRequest(input)), _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            // Start with a partial response.
            callback.Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucu"), /*is_complete=*/false,
                /*provided_by_on_device=*/true));
            saved_callback = callback;
          })));
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::PartialComposeResponsePtr>
      partial_future;
  EXPECT_CALL(compose_dialog(), PartialResponseReceived(_))
      .WillRepeatedly(testing::Invoke(
          [&](compose::mojom::PartialComposeResponsePtr response) {
            partial_future.SetValue(std::move(response));
          }));
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose(input, false);

  compose::mojom::PartialComposeResponsePtr partial_result =
      partial_future.Take();
  EXPECT_EQ("Cucu", partial_result->result);

  // Request the initial state, and verify there's still a pending request.
  base::test::TestFuture<compose::mojom::OpenMetadataPtr> initial_state_future;
  page_handler()->RequestInitialState(initial_state_future.GetCallback());
  compose::mojom::OpenMetadataPtr initial_state = initial_state_future.Take();
  EXPECT_TRUE(initial_state->compose_state->has_pending_request);

  // Then send the full response.
  saved_callback.Run(OptimizationGuideStreamingResult(
      ComposeResponse(true, "Cucumbers"), /*is_complete=*/true,
      /*provided_by_on_device=*/true));
  auto complete_result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, complete_result->status);
  EXPECT_EQ("Cucumbers", complete_result->result);
  EXPECT_TRUE(complete_result->on_device_evaluation_used);

  // Check that a single request result OK metric was emitted.
  histograms().ExpectUniqueSample(compose::kComposeRequestStatus,
                                  compose::mojom::ComposeStatus::kOk, 1);
  histograms().ExpectUniqueSample("Compose.OnDevice.Request.Status",
                                  compose::mojom::ComposeStatus::kOk, 1);
  // Check that a single request duration OK metric was emitted.
  histograms().ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationOkSuffix}), 1);
  histograms().ExpectTotalCount(
      base::StrCat(
          {"Compose.OnDevice", compose::kComposeRequestDurationOkSuffix}),
      1);
  // Check that no request duration Error metric was emitted.
  histograms().ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationErrorSuffix}),
      0);
}

TEST_F(ChromeComposeClientTest, TestComposeNoResultAnimation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationGuideOnDeviceModel}, {});

  const std::string input = "a user typed this";
  optimization_guide::proto::ComposeRequest context_request;
  *context_request.mutable_page_metadata() = ComposePageMetadata();
  base::test::TestFuture<
      optimization_guide::
          OptimizationGuideModelExecutionResultStreamingCallback>
      saved_callback;
  EXPECT_CALL(session(), AddContext(EqualsProto(context_request)));
  EXPECT_CALL(session(), ExecuteModel(EqualsProto(ComposeRequest(input)), _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) { saved_callback.SetValue(callback); })));
  ShowDialogAndBindMojo();

  EXPECT_CALL(compose_dialog(), PartialResponseReceived(_)).Times(0);
  EXPECT_CALL(compose_dialog(), ResponseReceived(_)).Times(1);

  page_handler()->Compose(input, false);

  // Send a partial response.
  saved_callback.Get().Run(OptimizationGuideStreamingResult(
      ComposeResponse(true, "Cucu"), /*is_complete=*/false,
      /*provided_by_on_device=*/true));

  // Then send the full response.
  saved_callback.Get().Run(OptimizationGuideStreamingResult(
      ComposeResponse(true, "Cucumbers"), /*is_complete=*/true,
      /*provided_by_on_device=*/true));
  FlushMojo();
}

TEST_F(ChromeComposeClientTest, TestComposeSessionIgnoresPreviousResponse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationGuideOnDeviceModel,
       compose::features::kComposeTextOutputAnimation},
      {});

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
                OptimizationGuideStreamingResult(ComposeResponse(true, "Cucu"),
                                                 /*is_complete=*/false));
          })));
  EXPECT_CALL(session(), ExecuteModel(EqualsProto(ComposeRequest(input2)), _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            // First call the original callback. This should be ignored.
            original_callback.Run(
                OptimizationGuideStreamingResult(ComposeResponse(true, "old")));
            // Start with a partial response.
            callback.Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucumbers")));
          })));
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::PartialComposeResponsePtr>
      partial_response;
  EXPECT_CALL(compose_dialog(), PartialResponseReceived(_))
      .WillRepeatedly(testing::Invoke(
          [&](compose::mojom::PartialComposeResponsePtr response) {
            partial_response.SetValue(std::move(response));
          }));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> complete_response;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            complete_response.SetValue(std::move(response));
          }));

  page_handler()->Compose(input, false);

  EXPECT_EQ("Cucu", partial_response.Get()->result);

  page_handler()->Compose(input2, false);
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk,
            complete_response.Get()->status);
  EXPECT_EQ("Cucumbers", complete_response.Get()->result);

  // Check that a single request result OK metric was emitted.
  histograms().ExpectUniqueSample(compose::kComposeRequestStatus,
                                  compose::mojom::ComposeStatus::kOk, 1);
  // Check that a single request duration OK metric was emitted.
  histograms().ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationOkSuffix}), 1);
  // Check that no request duration Error metric was emitted.
  histograms().ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationErrorSuffix}),
      0);
}

TEST_F(ChromeComposeClientTest, TestComposeRequestTimeout) {
  // Set config such that requests time out immediately.
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.request_latency_timeout_seconds = 0;

  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kRequestTimeout, result->status);
  histograms().ExpectUniqueSample(
      compose::kComposeRequestStatus,
      compose::mojom::ComposeStatus::kRequestTimeout, 1);
  histograms().ExpectUniqueSample(
      "Compose.Server.Request.Status",
      compose::mojom::ComposeStatus::kRequestTimeout, 1);
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
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucumbers")));
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

  NavigateAndCommitActiveTab(GURL("about:blank"));
}

TEST_F(ChromeComposeClientTest, TestComposeGenericServerError) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideModelStreamingExecutionResult(
                    base::unexpected(
                        OptimizationGuideModelExecutionError::
                            FromModelExecutionError(
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                    false,
                    std::make_unique<optimization_guide::ModelQualityLogEntry>(
                        std::make_unique<
                            optimization_guide::proto::LogAiDataRequest>(),
                        nullptr)));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

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

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kServerError, result->status);

  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  std::unique_ptr<optimization_guide::ModelQualityLogEntry> quality_result =
      quality_test_future.Take();

  // Check that the quality modeling log is still correct
  EXPECT_EQ(
      kSessionIdHigh,
      quality_result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->session_id()
          .high());
  EXPECT_EQ(
      kSessionIdLow,
      quality_result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->session_id()
          .low());

  // Check the expected event count metrics.
  std::vector<std::pair<compose::ComposeSessionEventTypes, int>> event_counts =
      {
          {compose::ComposeSessionEventTypes::kComposeDialogOpened, 1},
          {compose::ComposeSessionEventTypes::kMainDialogShown, 1},
          {compose::ComposeSessionEventTypes::kFREShown, 0},
          {compose::ComposeSessionEventTypes::kMSBBShown, 0},
          {compose::ComposeSessionEventTypes::kCreateClicked, 1},
          {compose::ComposeSessionEventTypes::kFailedRequest, 1},
      };

  for (auto [event_type, count] : event_counts) {
    histograms().ExpectBucketCount(compose::kComposeSessionEventCounts,
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.Server.Session.EventCounts",
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                   event_type, 0);
  }
}

TEST_F(ChromeComposeClientTest, TestComposeSetTriggeredFromModifierOnError) {
  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("", false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();

  // Simulate rewrite producing an error response.
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideModelStreamingExecutionResult(
                    base::unexpected(
                        OptimizationGuideModelExecutionError::
                            FromModelExecutionError(
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                    false,
                    std::make_unique<optimization_guide::ModelQualityLogEntry>(
                        std::make_unique<
                            optimization_guide::proto::LogAiDataRequest>(),
                        nullptr)));
          })));
  page_handler()->Rewrite(compose::mojom::StyleModifier::kRetry);

  result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kServerError, result->status);
  EXPECT_TRUE(result->triggered_from_modifier);

  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
}

// Tests that we return an error if Optimization Guide is unable to parse the
// response. In this case the response will be std::nullopt.
TEST_F(ChromeComposeClientTest, TestComposeNoParsedAny) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](OptimizationGuideModelExecutionResultStreamingCallback callback) {
            std::move(callback).Run(
                OptimizationGuideModelStreamingExecutionResult(
                    base::ok(StreamingResponse{.is_complete = true}),
                    /*provided_by_on_device=*/false));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kNoResponse, result->status);

  // Check that a request result No Response metric was emitted.
  histograms().ExpectUniqueSample(compose::kComposeRequestStatus,
                                  compose::mojom::ComposeStatus::kNoResponse,
                                  1);
  // Check that a request duration Error metric was emitted.
  histograms().ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationErrorSuffix}),
      1);
  // Check that no request duration OK metric was emitted.
  histograms().ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationOkSuffix}), 0);
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
          [&](OptimizationGuideModelExecutionResultStreamingCallback callback) {
            std::move(callback).Run(
                OptimizationGuideModelStreamingExecutionResult(
                    base::ok(OptimizationGuideResponse(
                        ComposeResponse(true, "Cucumbers"))),
                    false));
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

// Tests that a saved WebUI state is properly returned.
TEST_F(ChromeComposeClientTest, TestSaveAndRestoreWebUIState) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> test_future;

  page_handler()->SaveWebUIState("web ui state");
  page_handler()->RequestInitialState(test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = test_future.Take();
  EXPECT_EQ("web ui state", result->compose_state->webui_state);
}

// Tests that the same saved WebUI state is returned after compose().
TEST_F(ChromeComposeClientTest, TestSaveThenComposeThenRestoreWebUIState) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucumbers")));
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
  NavigateAndCommitActiveTab(GURL(chrome::kChromeUIUntrustedComposeUrl));
  // We skip showing the dialog here as there is no dialog required at this URL.
  BindMojo();

  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucumbers")));
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

// Tests that session level UKM metrics are properly captured after closing the
// dialog.
TEST_F(ChromeComposeClientTest, TestCancelUkmMetrics) {
  ShowDialogAndBindMojo();
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
  // Make sure the async call to CloseUI completes before navigating away.
  FlushMojo();

  // Navigate page away to upload UKM metrics to the collector.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check session level UKM metrics.
  auto session_ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_SessionProgress::kEntryName,
      {ukm::builders::Compose_SessionProgress::kCanceledName});

  EXPECT_EQ(session_ukm_entries.size(), 1UL);

  EXPECT_THAT(session_ukm_entries[0].metrics,
              testing::UnorderedElementsAre(testing::Pair(
                  ukm::builders::Compose_SessionProgress::kCanceledName, 1)));
}

// Tests that closing the session at chrome-untrusted://compose does not crash
// the browser, even though there is no dialog shown at that URL.
TEST_F(ChromeComposeClientTest, TestCloseUIAtChromeCompose) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeUIUntrustedComposeUrl));
  // We skip showing the dialog here as there is no dialog required at this
  // URL.
  BindMojo();
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
}

// Tests that an unpaired high surrogate resulting from truncation by substr is
// properly removed.
TEST_F(ChromeComposeClientTest, TestOpenDialogWithTruncatedSelectedText) {
  std::u16string input(u".🦄🦄🦄");
  field_data().set_value(input);
  SetSelectionWithTruncation(input, 6);
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ(".🦄🦄", result->initial_input);
}

// Tests that opening the dialog with user selected text will return that text
// when the WebUI requests initial state.
TEST_F(ChromeComposeClientTest, TestOpenDialogWithSelectedText) {
  field_data().set_value(u"user selected text");
  SetSelection(u"selected text");
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("selected text", result->initial_input);

  // Close session to record UMA
  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  // Check Compose Session Event Counts.
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kStartedWithSelection, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kInsertClicked, 1);
}

// Tests that opening the dialog with selected text from the proactive nudge
// will send that text to the WebUI dialog.
TEST_F(ChromeComposeClientTest,
       TestOpenDialogWithSelectedTextFromProactiveNudge) {
  field_data().set_value(u"user selected text");
  SetSelection(u"selected text");
  ShowDialogAndBindMojoWithFieldData(
      field_data(), base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("selected text", result->initial_input);

  // Close session to record UMA
  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  // Check Compose Session Event Counts.
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kStartedWithSelection, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kInsertClicked, 1);

  // Check Compose.ProactiveNudge.CTR metrics.
  histograms().ExpectBucketCount(
      compose::kComposeProactiveNudgeCtr,
      compose::ComposeProactiveNudgeCtrEvent::kDialogOpened, 1);
}

// Test that opening the saved state dialog with selected text does not start
// a new session or update the initial selection.
TEST_F(ChromeComposeClientTest, TestSelectedTextWithSavedStateNudge) {
  field_data().set_value(u"this text is first and this text is second");
  SetSelection(u"text is first");
  ShowDialogAndBindMojo();
  page_handler()->SaveWebUIState("web ui state");
  // Flush mojo before next dialog open call so that web ui state is preserved.
  FlushMojo();

  // Change selection and re-open dialog from saved state popup. The new
  // selection should be ignored.
  SetSelection(u"text is second");
  ShowDialogAndBindMojoWithFieldData(
      field_data(), base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("web ui state", result->compose_state->webui_state);
  EXPECT_EQ("text is first", result->initial_input);
  EXPECT_TRUE(result->text_selected);
}

TEST_F(ChromeComposeClientTest,
       TestMultipleDialogOpensWithChangingSelectedText) {
  field_data().set_value(u"this text is first and this text is second");
  SetSelection(u"text is first");
  ShowDialogAndBindMojo();
  page_handler()->SaveWebUIState("web ui state");
  // Flush mojo before next dialog open call so that web ui state is preserved.
  FlushMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();

  EXPECT_EQ("web ui state", result->compose_state->webui_state);
  EXPECT_EQ("text is first", result->initial_input);
  EXPECT_TRUE(result->text_selected);

  // Clear selection and re-open dialog from saved state popup.
  SetSelection(u"");
  ShowDialogAndBindMojoWithFieldData(
      field_data(), base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);

  page_handler()->RequestInitialState(open_test_future.GetCallback());
  result = open_test_future.Take();

  EXPECT_EQ("web ui state", result->compose_state->webui_state);
  EXPECT_EQ("text is first", result->initial_input);
  // Web UI should now show that no text was selected when the dialog opened.
  EXPECT_FALSE(result->text_selected);
}

// Tests that opening the dialog with selected text clears existing state.
TEST_F(ChromeComposeClientTest, TestClearStateWhenOpenWithSelectedText) {
  ShowDialogAndBindMojo();
  page_handler()->SaveWebUIState("web ui state");
  // Flush mojo before next dialog open call so that web ui state is preserved.
  FlushMojo();

  field_data().set_value(u"user selected text");
  SetSelection(u"selected text");
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());

  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_EQ("", result->compose_state->webui_state);
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Compose.EndedSession.NewSessionWithSelectedText"));
  histograms().ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kNewSessionWithSelectedText, 1);
}

TEST_F(ChromeComposeClientTest,
       TestContextMenuNotRecordedAsProactiveInQualityLogs) {
  field_data().set_value(u"user selected text");
  ShowDialogAndBindMojoWithFieldData(
      field_data(), base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kContextMenu);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("a user typed this", false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();
  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  std::unique_ptr<optimization_guide::ModelQualityLogEntry> quality_result =
      quality_test_future.Take();
  EXPECT_FALSE(
      quality_result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->started_with_proactive_nudge());

  // Force reporting of page events UKM.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check that page events UKM is recorded for opening from the nudge.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kComposeTextInsertedName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeOpenedName});

  EXPECT_EQ(ukm_entries.size(), 1UL);

  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeOpenedName,
              0)));
}

TEST_F(ChromeComposeClientTest, TestProactiveNudgeRecordedInQualityLogs) {
  field_data().set_value(u"user selected text");
  ShowDialogAndBindMojoWithFieldData(
      field_data(), base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("a user typed this", false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();
  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  std::unique_ptr<optimization_guide::ModelQualityLogEntry> quality_result =
      quality_test_future.Take();
  EXPECT_TRUE(
      quality_result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->started_with_proactive_nudge());

  // Force reporting of page events UKM.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check that page events UKM does not record opening the proactive nudge.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kComposeTextInsertedName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeOpenedName});

  EXPECT_EQ(ukm_entries.size(), 1UL);

  EXPECT_THAT(
      ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(
              ukm::builders::Compose_PageEvents::kComposeTextInsertedName, 1),
          testing::Pair(
              ukm::builders::Compose_PageEvents::kProactiveNudgeOpenedName,
              1)));
}

// Checks proper propagation of Compose config params.
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

// Tests that Undo is not possible when Compose is never called and no response
// is ever received.
TEST_F(ChromeComposeClientTest, TestEmptyUndo) {
  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::ComposeStatePtr> test_future;
  page_handler()->Undo(test_future.GetCallback());
  EXPECT_FALSE(test_future.Take());
}

// Tests that Undo is not possible after only one Compose() invocation.
// TODO(b/334007229): incorporate redo testing.
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

// Tests Undo after calling Compose() twice.
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

  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
  // Make sure the async call to CloseUI() completes before navigating away.
  FlushMojo();

  // Check Compose Session Event Counts.
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kUndoClicked, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kCloseClicked, 1);

  // Navigate page away to upload UKM metrics to the collector.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check session level UKM metrics.
  auto session_ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_SessionProgress::kEntryName,
      {ukm::builders::Compose_SessionProgress::kUndoCountName});

  EXPECT_EQ(session_ukm_entries.size(), 1UL);

  EXPECT_THAT(session_ukm_entries[0].metrics,
              testing::UnorderedElementsAre(testing::Pair(
                  ukm::builders::Compose_SessionProgress::kUndoCountName, 1)));
}

// Tests if Undo can be done more than once.
// TODO(b/334007229): incorporate redo testing.
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

// Tests scenario: Undo returns state A, Compose, then undo again returns to
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

// Tests that the corresponding callback is run when AcceptComposeResponse is
// called.
TEST_F(ChromeComposeClientTest, TestAcceptComposeResultCallback) {
  base::test::TestFuture<const std::u16string&> accept_callback;
  ShowDialogAndBindMojo(accept_callback.GetCallback());

  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucumbers")));
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
  // This test uses web_contents->GetController()->GetPendingEntry() as it only
  // verifies that a navigation has started, regardless of whether it commits or
  // not.
  content::WebContents* new_tab_webcontents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(bug_url,
            new_tab_webcontents->GetController().GetPendingEntry()->GetURL());
}

TEST_F(ChromeComposeClientTest, LearnMoreLinkOpensCorrectURL) {
  GURL learn_more_url("https://support.google.com/chrome?p=help_me_write");

  ShowDialogAndBindMojo();

  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());
  page_handler()->OpenComposeLearnMorePage();

  // Wait for the resulting new tab to be created.
  tab_add_waiter.Wait();
  // Check that the new foreground tab is opened.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // This test uses web_contents->GetController()->GetPendingEntry() as it only
  // verifies that a navigation has started, regardless of whether it commits or
  // not.
  content::WebContents* new_tab_webcontents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(learn_more_url,
            new_tab_webcontents->GetController().GetPendingEntry()->GetURL());
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
  // This test uses web_contents->GetController()->GetPendingEntry() as it only
  // verifies that a navigation has started, regardless of whether it commits or
  // not.
  content::WebContents* new_tab_webcontents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(survey_url,
            new_tab_webcontents->GetController().GetPendingEntry()->GetURL());
}

// Tests that all ComposeSessions are deleted on page navigation.
TEST_F(ChromeComposeClientTest, ResetClientOnNavigation) {
  ShowDialogAndBindMojo();

  page_handler()->SaveWebUIState("first state");
  page_handler()->Compose("", false);

  autofill::FormFieldData field_2;
  field_2.set_renderer_id(autofill::FieldRendererId(2));
  ShowDialogAndBindMojoWithFieldData(field_2);

  // There should be two sessions.
  EXPECT_EQ(2, client().GetSessionCountForTest());

  // Navigate to a new page.
  GURL next_page("http://example.com/a.html");
  NavigateAndCommit(web_contents(), next_page);

  // All sessions should be deleted.
  EXPECT_EQ(0, client().GetSessionCountForTest());
}

// Tests that the dialog close button logs to the correct corresponding
// histograms.
TEST_F(ChromeComposeClientTest, CloseButtonHistogramTest) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  // Simulate three Compose requests - two from edits.
  page_handler()->Compose("", false);
  compose::mojom::ComposeResponsePtr response = compose_future.Take();

  page_handler()->Compose("", true);
  response = compose_future.Take();

  page_handler()->Compose("", true);
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

  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Compose.EndedSession.CloseButtonClicked"));
  histograms().ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCloseButtonPressed, 1);

  // Expect that three total Compose calls were recorded.
  histograms().ExpectUniqueSample(
      compose::kComposeSessionComposeCount + std::string(".Ignored"), 3, 1);
  histograms().ExpectUniqueSample("Compose.Server.Session.ComposeCount.Ignored",
                                  3, 1);

  // Expect that two of the Compose calls were from edits.
  histograms().ExpectUniqueSample(
      compose::kComposeSessionUpdateInputCount + std::string(".Ignored"), 2, 1);
  histograms().ExpectUniqueSample(
      "Compose.Server.Session.SubmitEditCount.Ignored", 2, 1);

  // Expect that two undos were done.
  histograms().ExpectUniqueSample(
      compose::kComposeSessionUndoCount + std::string(".Ignored"), 2, 1);
  histograms().ExpectUniqueSample("Compose.Server.Session.UndoCount.Ignored", 2,
                                  1);

  // Expect that the dialog was shown twice.
  histograms().ExpectUniqueSample(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 2, 1);
  histograms().ExpectUniqueSample(
      "Compose.Server.Session.DialogShownCount.Ignored", 2, 1);

  // Check expected session duration metrics
  histograms().ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".FRE"), 0);
  histograms().ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".MSBB"), 0);
  histograms().ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".Ignored"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms().ExpectUniqueTimeSample(
      "Compose.Server.Session.Duration.Ignored",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms().ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);

  // Check the expected event count metrics.
  std::vector<std::pair<compose::ComposeSessionEventTypes, int>> event_counts =
      {
          {compose::ComposeSessionEventTypes::kComposeDialogOpened, 1},
          {compose::ComposeSessionEventTypes::kMainDialogShown, 1},
          {compose::ComposeSessionEventTypes::kFREShown, 0},
          {compose::ComposeSessionEventTypes::kCreateClicked, 1},
          {compose::ComposeSessionEventTypes::kSuccessfulRequest, 1},
          {compose::ComposeSessionEventTypes::kUpdateClicked, 1},
          {compose::ComposeSessionEventTypes::kUndoClicked, 1},
          {compose::ComposeSessionEventTypes::kAnyModifierUsed, 0},
          {compose::ComposeSessionEventTypes::kFailedRequest, 0},
      };

  for (auto [event_type, count] : event_counts) {
    histograms().ExpectBucketCount(compose::kComposeSessionEventCounts,
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.Server.Session.EventCounts",
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                   event_type, 0);
  }

  // No FRE related close reasons should have been recorded.
  histograms().ExpectTotalCount(compose::kComposeFirstRunSessionCloseReason, 0);

  // No MSBB related close reasons should have been recorded.
  histograms().ExpectTotalCount(compose::kComposeMSBBSessionCloseReason, 0);
}

TEST_F(ChromeComposeClientTest, CloseButtonMSBBHistogramTest) {
  SetPrefsForComposeMSBBState(false);
  ShowDialogAndBindMojo();

  client().CloseUI(compose::mojom::CloseReason::kMSBBCloseButton);

  histograms().ExpectUniqueSample(
      compose::kComposeMSBBSessionCloseReason,
      compose::ComposeMSBBSessionCloseReason::kMSBBCloseButtonPressed, 1);

  histograms().ExpectUniqueSample(
      compose::kComposeMSBBSessionDialogShownCount + std::string(".Ignored"),
      1,  // Expect that one total MSBB dialog was shown.
      1);
  histograms().ExpectTotalCount(compose::kComposeMSBBSessionCloseReason, 1);

  // No FRE related close reasons should have been recorded.
  histograms().ExpectTotalCount(compose::kComposeFirstRunSessionCloseReason, 0);

  // Check expected session duration metrics
  histograms().ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".FRE"), 0);
  histograms().ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".MSBB"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms().ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".Inserted"), 0);
  histograms().ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);

  // Check the expected event count metrics.
  std::vector<std::pair<compose::ComposeSessionEventTypes, int>> event_counts =
      {
          {compose::ComposeSessionEventTypes::kComposeDialogOpened, 1},
          {compose::ComposeSessionEventTypes::kMainDialogShown, 0},
          {compose::ComposeSessionEventTypes::kFREShown, 0},
          {compose::ComposeSessionEventTypes::kMSBBShown, 1},
          {compose::ComposeSessionEventTypes::kFREAccepted, 0},
          {compose::ComposeSessionEventTypes::kMSBBEnabled, 0},
      };

  for (auto [event_type, count] : event_counts) {
    histograms().ExpectBucketCount(compose::kComposeSessionEventCounts,
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.Server.Session.EventCounts",
                                   event_type, 0);
    histograms().ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                   event_type, 0);
  }
}

TEST_F(ChromeComposeClientTest,
       CloseButtonMSBBEnabledDuringSessionHistogramTest) {
  SetPrefsForComposeMSBBState(false);
  ShowDialogAndBindMojo();

  SetPrefsForComposeMSBBState(true);
  // Show the dialog a second time.
  ShowDialogAndBindMojo();

  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  histograms().ExpectUniqueSample(
      compose::kComposeSessionComposeCount + std::string(".Ignored"),
      0,  // Expect that zero total Compose calls were recorded.
      1);

  histograms().ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCloseButtonPressed, 1);

  histograms().ExpectUniqueSample(
      compose::kComposeMSBBSessionCloseReason,
      compose::ComposeMSBBSessionCloseReason::kMSBBAcceptedWithoutInsert, 1);

  histograms().ExpectUniqueSample(
      compose::kComposeMSBBSessionDialogShownCount + std::string(".Accepted"),
      1,  // Expect that the dialog was shown once.
      1);
  histograms().ExpectTotalCount(compose::kComposeMSBBSessionCloseReason, 1);

  // No FRE related close reasons should have been recorded.
  histograms().ExpectTotalCount(compose::kComposeFirstRunSessionCloseReason, 0);

  // Check the expected event count metrics.
  std::vector<std::pair<compose::ComposeSessionEventTypes, int>> event_counts =
      {
          {compose::ComposeSessionEventTypes::kComposeDialogOpened, 1},
          {compose::ComposeSessionEventTypes::kMainDialogShown, 1},
          {compose::ComposeSessionEventTypes::kFREShown, 0},
          {compose::ComposeSessionEventTypes::kFREAccepted, 0},
          {compose::ComposeSessionEventTypes::kMSBBShown, 1},
          {compose::ComposeSessionEventTypes::kMSBBEnabled, 1},
          {compose::ComposeSessionEventTypes::kInsertClicked, 0},
          {compose::ComposeSessionEventTypes::kCloseClicked, 1},
      };

  for (auto [event_type, count] : event_counts) {
    histograms().ExpectBucketCount(compose::kComposeSessionEventCounts,
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.Server.Session.EventCounts",
                                   event_type, 0);
    histograms().ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                   event_type, 0);
  }
}

TEST_F(ChromeComposeClientTest, FirstRunCloseDialogHistogramTest) {
  // Enable FRE and show the dialog.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  ShowDialogAndBindMojo();
  client().CloseUI(compose::mojom::CloseReason::kFirstRunCloseButton);
  histograms().ExpectUniqueSample(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFirstRunSessionCloseReason::kCloseButtonPressed, 1);
  // Expect that the dialog was shown once ending without FRE completed.
  histograms().ExpectUniqueSample(
      compose::kComposeFirstRunSessionDialogShownCount +
          std::string(".Ignored"),
      1, 1);

  // Check expected session duration metrics.
  histograms().ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".FRE"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms().ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".MSBB"), 0);
  histograms().ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".Ignored"), 0);
  histograms().ExpectTotalCount("Compose.Server.Session.Duration.Ignored", 0);
  histograms().ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);

  // Check the expected event count metrics.
  std::vector<std::pair<compose::ComposeSessionEventTypes, int>> event_counts =
      {
          {compose::ComposeSessionEventTypes::kComposeDialogOpened, 1},
          {compose::ComposeSessionEventTypes::kMainDialogShown, 0},
          {compose::ComposeSessionEventTypes::kFREShown, 1},
          {compose::ComposeSessionEventTypes::kFREAccepted, 0},
          {compose::ComposeSessionEventTypes::kMSBBShown, 0},
          {compose::ComposeSessionEventTypes::kMSBBEnabled, 0},
      };

  for (auto [event_type, count] : event_counts) {
    histograms().ExpectBucketCount(compose::kComposeSessionEventCounts,
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.Server.Session.EventCounts",
                                   event_type, 0);
    histograms().ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                   event_type, 0);
  }

  // Show the FRE dialog and end the session by re-opening with selection.
  ShowDialogAndBindMojo();
  field_data().set_value(u"user selected text");
  SetSelection(u"selected text");
  ShowDialogAndBindMojo();
  histograms().ExpectBucketCount(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFirstRunSessionCloseReason::kNewSessionWithSelectedText,
      1);
  histograms().ExpectBucketCount(
      compose::kComposeFirstRunSessionDialogShownCount +
          std::string(".Ignored"),
      1,  // Expect that the dialog was shown once.
      2);

  // Throughout all sessions no main dialog metrics should have been logged, as
  // the dialog never moved past the FRE.
  histograms().ExpectTotalCount(compose::kComposeSessionCloseReason, 0);
  histograms().ExpectTotalCount(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 0);
}

TEST_F(ChromeComposeClientTest, FirstRunCompletedHistogramTest) {
  // Enable FRE and show the dialog.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  ShowDialogAndBindMojo();
  // Show the dialog a second time.
  ShowDialogAndBindMojo();
  // Complete FRE and close.
  client().CompleteFirstRun();
  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  histograms().ExpectUniqueSample(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFirstRunSessionCloseReason::
          kFirstRunDisclaimerAcknowledgedWithoutInsert,
      1);
  // Expect that the dialog was shown twice ending with FRE completed.
  histograms().ExpectUniqueSample(
      compose::kComposeFirstRunSessionDialogShownCount +
          std::string(".Acknowledged"),
      2, 1);

  // After FRE is completed, a new set of metrics should be collected for the
  // remainder of the session.
  histograms().ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCloseButtonPressed, 1);
  histograms().ExpectUniqueSample(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"),
      1,  // The dialog was only shown once after having proceeded past FRE.
      1);

  // Check the expected event count metrics.
  std::vector<std::pair<compose::ComposeSessionEventTypes, int>> event_counts =
      {
          {compose::ComposeSessionEventTypes::kComposeDialogOpened, 1},
          {compose::ComposeSessionEventTypes::kMainDialogShown, 1},
          {compose::ComposeSessionEventTypes::kFREShown, 1},
          {compose::ComposeSessionEventTypes::kFREAccepted, 1},
          {compose::ComposeSessionEventTypes::kMSBBShown, 0},
          {compose::ComposeSessionEventTypes::kMSBBEnabled, 0},
      };

  for (auto [event_type, count] : event_counts) {
    histograms().ExpectBucketCount(compose::kComposeSessionEventCounts,
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.Server.Session.EventCounts",
                                   event_type, 0);
    histograms().ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                   event_type, 0);
  }
}

TEST_F(ChromeComposeClientTest,
       FirstRunCompletedThenSuggestionAcceptedHistogramTest) {
  // Enable FRE and show the dialog.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  ShowDialogAndBindMojo();
  // Complete FRE then close by inserting.
  client().CompleteFirstRun();
  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  histograms().ExpectUniqueSample(compose::kComposeFirstRunSessionCloseReason,
                                  compose::ComposeFirstRunSessionCloseReason::
                                      kFirstRunDisclaimerAcknowledgedWithInsert,
                                  1);

  // Check the expected session event count metrics.
  std::vector<std::pair<compose::ComposeSessionEventTypes, int>> event_counts =
      {
          {compose::ComposeSessionEventTypes::kComposeDialogOpened, 1},
          {compose::ComposeSessionEventTypes::kMainDialogShown, 1},
          {compose::ComposeSessionEventTypes::kFREShown, 1},
          {compose::ComposeSessionEventTypes::kFREAccepted, 1},
          {compose::ComposeSessionEventTypes::kMSBBShown, 0},
          {compose::ComposeSessionEventTypes::kMSBBEnabled, 0},
          {compose::ComposeSessionEventTypes::kStartedWithSelection, 0},
          {compose::ComposeSessionEventTypes::kInsertClicked, 1},
      };

  for (auto [event_type, count] : event_counts) {
    histograms().ExpectBucketCount(compose::kComposeSessionEventCounts,
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.Server.Session.EventCounts",
                                   event_type, 0);
    histograms().ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                   event_type, 0);
  }
}

TEST_F(ChromeComposeClientTest, CompleteFirstRunTest) {
  // Enable FRE and show the dialog.
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kPrefHasCompletedComposeFRE, false);

  ShowDialogAndBindMojo();
  client().CompleteFirstRun();

  EXPECT_TRUE(prefs->GetBoolean(prefs::kPrefHasCompletedComposeFRE));

  // Make sure the async calls complete before naviagating away.
  FlushMojo();
  // Navigate page away to upload session close metrics.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check the expected event count metrics.
  std::vector<std::pair<compose::ComposeSessionEventTypes, int>> event_counts =
      {
          {compose::ComposeSessionEventTypes::kComposeDialogOpened, 1},
          {compose::ComposeSessionEventTypes::kMainDialogShown, 1},
          {compose::ComposeSessionEventTypes::kFREShown, 1},
          {compose::ComposeSessionEventTypes::kMSBBShown, 0},
          {compose::ComposeSessionEventTypes::kCreateClicked, 0},
      };

  for (auto [event_type, count] : event_counts) {
    histograms().ExpectBucketCount(compose::kComposeSessionEventCounts,
                                   event_type, count);
    histograms().ExpectBucketCount("Compose.Server.Session.EventCounts",
                                   event_type, 0);
    histograms().ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                   event_type, 0);
  }
}

TEST_F(ChromeComposeClientTest,
       AddSiteToNeverPromptListBlocksProactiveNudgeTest) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_show_probability = 1.0;
  config.proactive_nudge_delay = base::Seconds(0);
  config.proactive_nudge_segmentation = false;

  PrefService* prefs = GetProfile()->GetPrefs();

  auto test_url = GURL("http://foo");
  auto test_origin = url::Origin::Create(test_url);

  autofill::FormData form_data;
  form_data.set_url(test_url);
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  autofill::FormFieldData selected_field_data = form_data.fields[0];
  selected_field_data.set_origin(test_origin);
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange;

  EXPECT_FALSE(prefs->GetDict(prefs::kProactiveNudgeDisabledSitesWithTime)
                   .Find(test_origin.Serialize()));
  EXPECT_TRUE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                          trigger_source));

  client().AddSiteToNeverPromptList(test_origin);

  EXPECT_TRUE(prefs->GetDict(prefs::kProactiveNudgeDisabledSitesWithTime)
                  .Find(test_origin.Serialize()));
  EXPECT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));
}

TEST_F(ChromeComposeClientTest, TextFieldChangeThresholdHidesProactiveNudge) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_show_probability = 1.0;
  config.proactive_nudge_delay = base::Seconds(0);
  config.proactive_nudge_segmentation = false;

  client().field_change_observer_.SetSkipSuggestionTypeForTest(true);

  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.fields = {autofill::test::CreateTestFormField(
      "label0", "name0", "value0", autofill::FormControlType::kTextArea)};

  // Simulate an Autofill popup being shown.
  autofill::AutofillClient::PopupOpenArgs args;
  args.suggestions = {
      autofill::Suggestion(autofill::SuggestionType::kComposeProactiveNudge)};
  autofill_client()->ShowAutofillSuggestions(args, /*delegate=*/nullptr);
  EXPECT_TRUE(autofill_client()->IsShowingAutofillPopup());

  // Simulate field change events up to limit specified by config.
  std::u16string text_value = u"a";
  unsigned int max = config.nudge_field_change_event_max;
  for (size_t i = 1; i < max; i++) {
    client().field_change_observer_.OnAfterTextFieldDidChange(
        *autofill_manager(), form_data.global_id(),
        form_data.fields[0].global_id(), text_value);
    EXPECT_EQ(i,
              client().field_change_observer_.text_field_change_event_count_);
    text_value = text_value + u"a";
  }

  // Reaching the event threshold resets the event count and hides the Autofill
  // popup.
  client().field_change_observer_.OnAfterTextFieldDidChange(
      *autofill_manager(), form_data.global_id(),
      form_data.fields[0].global_id(), text_value);
  EXPECT_EQ(0U, client().field_change_observer_.text_field_change_event_count_);
  EXPECT_FALSE(autofill_client()->IsShowingAutofillPopup());
}

TEST_F(ChromeComposeClientTest, AcceptSuggestionHistogramTest) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  // Simulate three compose requests - two from edits.
  page_handler()->Compose("", false);
  compose::mojom::ComposeResponsePtr response = compose_future.Take();

  page_handler()->Compose("", true);
  response = compose_future.Take();

  page_handler()->Compose("", true);
  response = compose_future.Take();

  // Show the dialog a second time.
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();

  // Show the dialog a third time.
  ShowDialogAndBindMojo();

  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Compose.EndedSession.InsertButtonClicked"));
  histograms().ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kAcceptedSuggestion, 1);
  histograms().ExpectUniqueSample(
      compose::kComposeSessionComposeCount + std::string(".Accepted"),
      3,  // Expect that three Compose calls were recorded.
      1);
  histograms().ExpectUniqueSample(
      compose::kComposeSessionUpdateInputCount + std::string(".Accepted"),
      2,  // Expect that two of the Compose calls were from edits.
      1);
  histograms().ExpectUniqueSample(
      compose::kComposeSessionUndoCount + std::string(".Accepted"),
      1,  // Expect that one undo was done.
      1);
  histograms().ExpectUniqueSample(
      compose::kComposeSessionDialogShownCount + std::string(".Accepted"),
      3,  // Expect that the dialog was shown three times.
      1);
  histograms().ExpectUniqueSample(
      "Compose.Server.Session.DialogShownCount.Accepted",
      3,  // Expect that the dialog was shown three times.
      1);

  // Check expected session duration metrics.
  histograms().ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".FRE"), 0);
  histograms().ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".MSBB"), 0);
  histograms().ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".Inserted"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms().ExpectUniqueTimeSample(
      "Compose.Server.Session.Duration.Inserted",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms().ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);
}

TEST_F(ChromeComposeClientTest, LoseFocusHistogramTest) {
  ShowDialogAndBindMojo();

  // Dismiss dialog by losing focus by navigating.
  GURL next_page("http://example.com/a.html");
  NavigateAndCommit(web_contents(), next_page);

  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Compose.EndedSession.EndedImplicitly"));
  histograms().ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kEndedImplicitly, 1);
}

TEST_F(ChromeComposeClientTest, LoseFocusFirstRunHistogramTest) {
  // Enable FRE and show the dialog.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  ShowDialogAndBindMojo();

  // Dismiss dialog by losing focus by navigating.
  GURL next_page("http://example.com/a.html");
  NavigateAndCommit(web_contents(), next_page);

  histograms().ExpectUniqueSample(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFirstRunSessionCloseReason::kEndedImplicitly, 1);
}

TEST_F(ChromeComposeClientTest, ComposeDialogStatesSeenUserActionsTest) {
  // Set both FRE and MSBB dialog states to show and check that appropriate
  // user actions are logged when moving through all states in a single session.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  SetPrefsForComposeMSBBState(false);
  EXPECT_EQ(0, user_action_tester().GetActionCount(
                   "Compose.DialogSeen.FirstRunDisclaimer"));
  EXPECT_EQ(0, user_action_tester().GetActionCount(
                   "Compose.DialogSeen.FirstRunMSBB"));
  EXPECT_EQ(
      0, user_action_tester().GetActionCount("Compose.DialogSeen.MainDialog"));

  // Dialog should show at FRE state.
  ShowDialogAndBindMojo();
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Compose.DialogSeen.FirstRunDisclaimer"));
  // After acknowledging the disclaimer, dialog should show the MSBB state.
  client().CompleteFirstRun();
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Compose.DialogSeen.FirstRunMSBB"));
  // After updating the MSBB setting, only the next open of the dialog should
  // record  a dialog seen action.
  SetPrefsForComposeMSBBState(true);
  ShowDialogAndBindMojo();
  EXPECT_EQ(
      1, user_action_tester().GetActionCount("Compose.DialogSeen.MainDialog"));
  // Show dialog again.
  ShowDialogAndBindMojo();
  EXPECT_EQ(
      1, user_action_tester().GetActionCount("Compose.DialogSeen.MainDialog"));
  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Check user actions for new session opened at MSBB state.
  SetPrefsForComposeMSBBState(false);
  ShowDialogAndBindMojo();
  EXPECT_EQ(2, user_action_tester().GetActionCount(
                   "Compose.DialogSeen.FirstRunMSBB"));
  client().CloseUI(compose::mojom::CloseReason::kMSBBCloseButton);

  // Check user actions for new session opened at main dialog state.
  SetPrefsForComposeMSBBState(true);
  ShowDialogAndBindMojo();
  EXPECT_EQ(
      2, user_action_tester().GetActionCount("Compose.DialogSeen.MainDialog"));
  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Check user actions for session opened at FRE state and progressing directly
  // to main dialog state.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  ShowDialogAndBindMojo();
  EXPECT_EQ(2, user_action_tester().GetActionCount(
                   "Compose.DialogSeen.FirstRunDisclaimer"));
  // After acknowledging the disclaimer, dialog should show the main state.
  client().CompleteFirstRun();
  EXPECT_EQ(
      3, user_action_tester().GetActionCount("Compose.DialogSeen.MainDialog"));
}

TEST_F(ChromeComposeClientTest, TestAutoCompose) {
  EnableAutoCompose();
  base::test::TestFuture<void> execute_model_future;
  // Make model execution hang.
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(base::test::RunOnceClosure(execute_model_future.GetCallback()));

  std::u16string selected_text = u"ŧëśŧĩňĝ âľpħâ ƅřâɤō ĉħâŗľĩë";
  std::string selected_text_utf8 = base::UTF16ToUTF8(selected_text);
  SetSelection(selected_text);
  ShowDialogAndBindMojo();
  FlushMojo();

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

  // Check that opening from the context menu with an empty selection resumes
  // without autocompose.
  SetSelection(u"");
  // Would crash if Compose is called again since we expect ExecuteModel to run
  // just once.
  ShowDialogAndBindMojo();
  FlushMojo();

  // Check opening from the saved state menu with a selection resumes without
  // autocompose.
  SetSelection(u"Some new selected text");
  // Would crash if Compose is called again since we expect ExecuteModel to run
  // just once.
  ShowDialogAndBindMojoWithFieldData(
      field_data(), base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);
}

TEST_F(ChromeComposeClientTest, TestAutoComposeTooLong) {
  EnableAutoCompose();
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);

  std::u16string words(compose::GetComposeConfig().input_max_chars - 3, u'a');
  words += u" b c";
  SetSelection(words);
  ShowDialogAndBindMojo();

  histograms().ExpectUniqueSample(compose::kComposeDialogSelectionLength,
                                  base::UTF16ToUTF8(words).size(), 1);

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

TEST_F(ChromeComposeClientTest, TestAutoComposeTooFewWords) {
  EnableAutoCompose();
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
  EnableAutoCompose();
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
  // Auto compose is disabled by default.
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);

  SetSelection(u"testing alpha bravo charlie");
  ShowDialogAndBindMojo();
}

TEST_F(ChromeComposeClientTest, TestNoAutoComposeWithPopup) {
  EnableAutoCompose();
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);
  SetSelection(u"a");  // Too short to cause auto compose.

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
  EnableAutoCompose();
  base::test::TestFuture<void> execute_model_future;
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(base::test::RunOnceClosure(execute_model_future.GetCallback()));

  SetSelection(u"a");  // Too short to cause auto compose.

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

TEST_F(ChromeComposeClientTest, TestNoAutoComposeBeforeFirstRun) {
  EnableAutoCompose();
  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(0);

  // Enable FRE and show the dialog.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  // Valid selection for auto compose to use.
  std::u16string selection = u"testing alpha bravo charlie";
  SetSelection(selection);
  ShowDialogAndBindMojo();

  // Without FRE completion auto compose should not execute.
  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_FALSE(result->compose_state->has_pending_request);
}

// Tests that quality logs are uploaded when a new valid response clears forward
// state and when the session is destroyed, and that those logs have the
// expected session IDs attached.
TEST_F(ChromeComposeClientTest, TestComposeQualitySessionId) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(3);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillOnce(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed one", false);
  EXPECT_TRUE(compose_future.Wait());
  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed two", false);
  EXPECT_TRUE(compose_future.Wait());
  // Reset future for third compose call.
  compose_future.Clear();

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  // Undo reverts client to the first saved state in the history, with one
  // forward state resulting from the second compose.
  page_handler()->Undo(undo_future.GetCallback());
  EXPECT_TRUE(undo_future.Wait());

  // Third compose should clear the forward state from the second compose and
  // upload its corresponding quality logs.
  page_handler()->Compose("a user typed three", false);
  EXPECT_TRUE(compose_future.Wait());

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

  // Close UI should result in upload of quality logs for the two responses left
  // in the state history.
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future_2;
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future_3;
  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            if (!quality_test_future_2.IsReady()) {
              quality_test_future_2.SetValue(std::move(response));
            } else {
              quality_test_future_3.SetValue(std::move(response));
            }
          }));

  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  result = quality_test_future_2.Take();
  EXPECT_EQ(kSessionIdHigh,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->session_id()
                .high());
  EXPECT_EQ(kSessionIdLow,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->session_id()
                .low());

  result = quality_test_future_3.Take();
  EXPECT_EQ(kSessionIdHigh,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->session_id()
                .high());
  EXPECT_EQ(kSessionIdLow,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->session_id()
                .low());
}

TEST_F(ChromeComposeClientTest, TestComposeQualityLoggedOnSubsequentError) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillRepeatedly(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideModelStreamingExecutionResult(
                    base::unexpected(
                        OptimizationGuideModelExecutionError::
                            FromModelExecutionError(
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                    /*provided_by_on_device=*/false,
                    std::make_unique<optimization_guide::ModelQualityLogEntry>(
                        std::make_unique<
                            optimization_guide::proto::LogAiDataRequest>(),
                        nullptr)));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            compose_future.SetValue(std::move(response));
          }));

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

  compose::mojom::ComposeResponsePtr compose_result = compose_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kServerError,
            compose_result->status);

  page_handler()->Compose("a user typed that", false);

  compose_result = compose_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kServerError,
            compose_result->status);

  std::unique_ptr<optimization_guide::ModelQualityLogEntry> quality_result =
      quality_test_future.Take();

  // Ensure that a quality log is emitted after a second compose error.
  EXPECT_EQ(
      kSessionIdLow,
      quality_result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->session_id()
          .low());
  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  quality_result = quality_test_future.Take();

  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      quality_result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->request_latency_ms());

  // Check that histogram was sent for Compose State removed from undo stack.
  histograms().ExpectBucketCount("Compose.Server.Request.Feedback",
                                 compose::ComposeRequestFeedback::kNoFeedback,
                                 0);
  histograms().ExpectBucketCount("Compose.Server.Request.Feedback",
                                 compose::ComposeRequestFeedback::kRequestError,
                                 2);
}

// Tests that quality logs are uploaded when a new valid response clears forward
// state and when the session is destroyed, and that those logs have expected
// latency data attached.
TEST_F(ChromeComposeClientTest, TestComposeQualityLatency) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(3);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillOnce(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed one", false);
  EXPECT_TRUE(compose_future.Wait());
  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed two", false);
  EXPECT_TRUE(compose_future.Wait());
  // Reset future for third compose call.
  compose_future.Clear();

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  // Undo reverts client to the first saved state in the history, with one
  // forward state resulting from the second compose.
  page_handler()->Undo(undo_future.GetCallback());
  EXPECT_TRUE(undo_future.Wait());

  // Third compose should clear the forward state from the second compose and
  // upload its corresponding quality logs.
  page_handler()->Compose("a user typed three", false);
  EXPECT_TRUE(compose_future.Wait());

  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();
  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->request_latency_ms());

  // Close UI should result in upload of quality logs for the two responses left
  // in the state history.
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future_2;
  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future_3;
  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            if (!quality_test_future_2.IsReady()) {
              quality_test_future_2.SetValue(std::move(response));
            } else {
              quality_test_future_3.SetValue(std::move(response));
            }
          }));

  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  result = quality_test_future_2.Take();
  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->request_latency_ms());

  result = quality_test_future_3.Take();
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

  // This take should clear the quality future from the abandoned request.
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

TEST_F(ChromeComposeClientTest, TestComposeQualityNewSessionWithSelectedText) {
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
  EXPECT_TRUE(compose_future.Take());  // Reset future for second compose call.

  // Start a new session with selected text.
  field_data().set_value(u"user selected text");
  SetSelection(u"selected text");
  ShowDialogAndBindMojo();

  // Get quality result from the abandoned session.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();

  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_ABANDONED,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->final_status());

  page_handler()->Compose("a user typed that", false);
  EXPECT_TRUE(compose_future.Take());

  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  result = quality_test_future.Take();

  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_ABANDONED,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->final_status());
}

TEST_F(ChromeComposeClientTest, TestComposeQualitFinishedWithoutInsert) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _));

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillOnce(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose("a user typed this", false);
  EXPECT_TRUE(compose_future.Take());  // Reset future for second compose call.

  // Navigate to a new page.
  GURL next_page("http://example.com/a.html");
  NavigateAndCommit(web_contents(), next_page);

  // Get quality result from the abandoned session.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();

  EXPECT_EQ(
      optimization_guide::proto::FinalStatus::STATUS_FINISHED_WITHOUT_INSERT,
      result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
          ->final_status());
}

TEST_F(ChromeComposeClientTest, TestComposeQualityFeedbackPositive) {
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(1);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));

  ShowDialogAndBindMojo();
  client().GetSessionForActiveComposeField()->SetSkipFeedbackUiForTesting(true);

  page_handler()->Compose("a user typed this", false);
  ASSERT_TRUE(compose_future.Take());

  page_handler()->SetUserFeedback(
      compose::mojom::UserFeedback::kUserFeedbackPositive);

  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Get quality logs sent for the Compose Request.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();

  EXPECT_EQ(optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->user_feedback());

  // Check that the histogram was sent for request feedback.
  histograms().ExpectUniqueSample(
      "Compose.Server.Request.Feedback",
      compose::ComposeRequestFeedback::kPositiveFeedback, 1);
}

TEST_F(ChromeComposeClientTest, TestComposeQualityFeedbackNegative) {
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  EXPECT_CALL(session(), ExecuteModel(_, _)).Times(1);

  base::test::TestFuture<
      std::unique_ptr<optimization_guide::ModelQualityLogEntry>>
      quality_test_future;

  EXPECT_CALL(model_quality_logs_uploader(), UploadModelQualityLogs(_))
      .WillRepeatedly(testing::Invoke(
          [&](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                  response) {
            quality_test_future.SetValue(std::move(response));
          }));

  ShowDialogAndBindMojo();
  client().GetSessionForActiveComposeField()->SetSkipFeedbackUiForTesting(true);

  page_handler()->Compose("a user typed this", false);
  ASSERT_TRUE(compose_future.Take());

  page_handler()->SetUserFeedback(
      compose::mojom::UserFeedback::kUserFeedbackNegative);

  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Get quality logs sent for the Compose Request.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> result =
      quality_test_future.Take();

  EXPECT_EQ(optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->user_feedback());

  // Check that the histogram was sent for request feedback.
  histograms().ExpectUniqueSample(
      "Compose.Server.Request.Feedback",
      compose::ComposeRequestFeedback::kNegativeFeedback, 1);
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

  histograms().ExpectBucketCount(compose::kComposeRequestReason,
                                 compose::ComposeRequestReason::kFirstRequest,
                                 1);
  histograms().ExpectBucketCount("Compose.Server.Request.Reason",
                                 compose::ComposeRequestReason::kFirstRequest,
                                 1);
  histograms().ExpectBucketCount(compose::kComposeRequestReason,
                                 compose::ComposeRequestReason::kUpdateRequest,
                                 1);
  histograms().ExpectBucketCount("Compose.Server.Request.Reason",
                                 compose::ComposeRequestReason::kUpdateRequest,
                                 1);

  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_UNSPECIFIED,
            result->quality_data<optimization_guide::ComposeFeatureTypeMap>()
                ->final_status());
  // Check that the histogram was sent for request feedback.
  histograms().ExpectUniqueSample("Compose.Server.Request.Feedback",
                                  compose::ComposeRequestFeedback::kNoFeedback,
                                  2);
}

TEST_F(ChromeComposeClientTest, TestRegenerate) {
  ShowDialogAndBindMojo();
  std::string user_input = "a user typed this";
  auto matcher = EqualsProto(ComposeRequest(user_input));
  EXPECT_CALL(session(), ExecuteModel(matcher, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucumbers")));
          })));
  auto regen_matcher =
      EqualsProto(RegenerateRequest(/*previous_response=*/"Cucumbers"));
  EXPECT_CALL(session(), ExecuteModel(regen_matcher, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Tomatoes")));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose(user_input, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);

  page_handler()->Rewrite(compose::mojom::StyleModifier::kRetry);
  result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Tomatoes", result->result);

  histograms().ExpectBucketCount(compose::kComposeRequestReason,
                                 compose::ComposeRequestReason::kRetryRequest,
                                 1);
  histograms().ExpectBucketCount("Compose.Server.Request.Reason",
                                 compose::ComposeRequestReason::kRetryRequest,
                                 1);

  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Make sure the async call to CloseUI completes before navigating away.
  FlushMojo();

  // Check Compose Session Event Counts.
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kRetryClicked, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kCloseClicked, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kAnyModifierUsed, 0);

  // Navigate page away to upload UKM metrics to the collector.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check session level UKM metrics.
  auto session_ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_SessionProgress::kEntryName,
      {ukm::builders::Compose_SessionProgress::kRegenerateCountName});

  EXPECT_EQ(session_ukm_entries.size(), 1UL);

  EXPECT_THAT(
      session_ukm_entries[0].metrics,
      testing::UnorderedElementsAre(testing::Pair(
          ukm::builders::Compose_SessionProgress::kRegenerateCountName, 1)));
}

TEST_F(ChromeComposeClientTest, TestToneChange) {
  ShowDialogAndBindMojo();
  std::string user_input = "a user typed this";
  auto compose_matcher = EqualsProto(ComposeRequest(user_input));
  EXPECT_CALL(session(), ExecuteModel(compose_matcher, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucumbers")));
          })));
  // Rewrite with Formal.
  optimization_guide::proto::ComposeRequest request;
  request.mutable_rewrite_params()->set_previous_response("Cucumbers");
  request.mutable_rewrite_params()->set_tone(
      optimization_guide::proto::ComposeTone::COMPOSE_FORMAL);
  auto rewrite_matcher = EqualsProto(request);
  EXPECT_CALL(session(), ExecuteModel(rewrite_matcher, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Tomatoes")));
          })));
  // Rewrite with Casual.
  request.mutable_rewrite_params()->set_previous_response("Tomatoes");
  request.mutable_rewrite_params()->set_tone(
      optimization_guide::proto::ComposeTone::COMPOSE_INFORMAL);
  auto rewrite_matcher_informal = EqualsProto(request);
  EXPECT_CALL(session(), ExecuteModel(rewrite_matcher_informal, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Potatoes")));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose(user_input, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);

  page_handler()->Rewrite(compose::mojom::StyleModifier::kFormal);
  result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Tomatoes", result->result);
  histograms().ExpectBucketCount(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kToneFormalRequest, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kToneFormalRequest, 1);

  page_handler()->Rewrite(compose::mojom::StyleModifier::kCasual);
  result = test_future.Take();
  histograms().ExpectBucketCount(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kToneCasualRequest, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kToneCasualRequest, 1);

  // Make sure the async call to CloseUI completes before navigating away.
  FlushMojo();

  // Navigate page away to upload UKM metrics to the collector.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check Compose Session Event Counts.
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kFormalClicked, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kCasualClicked, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kElaborateClicked, 0);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kShortenClicked, 0);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kAnyModifierUsed, 1);

  // Check session level UKM metrics.
  auto session_ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_SessionProgress::kEntryName,
      {ukm::builders::Compose_SessionProgress::kCasualCountName,
       ukm::builders::Compose_SessionProgress::kFormalCountName});

  EXPECT_EQ(session_ukm_entries.size(), 1UL);

  EXPECT_THAT(
      session_ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kCasualCountName, 1),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kFormalCountName, 1)));
}

TEST_F(ChromeComposeClientTest, TestLengthChange) {
  ShowDialogAndBindMojo();
  std::string user_input = "a user typed this";
  auto compose_matcher = EqualsProto(ComposeRequest(user_input));
  EXPECT_CALL(session(), ExecuteModel(compose_matcher, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Cucumbers")));
          })));

  // Rewrite with Elaborate.
  optimization_guide::proto::ComposeRequest request;
  request.mutable_rewrite_params()->set_previous_response("Cucumbers");
  request.mutable_rewrite_params()->set_length(
      optimization_guide::proto::ComposeLength::COMPOSE_LONGER);
  auto rewrite_matcher = EqualsProto(request);
  EXPECT_CALL(session(), ExecuteModel(rewrite_matcher, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Tomatoes")));
          })));

  // Rewrite with Shorten.
  request.mutable_rewrite_params()->set_previous_response("Tomatoes");
  request.mutable_rewrite_params()->set_length(
      optimization_guide::proto::ComposeLength::COMPOSE_SHORTER);
  auto rewrite_shorten_matcher = EqualsProto(request);
  EXPECT_CALL(session(), ExecuteModel(rewrite_shorten_matcher, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(OptimizationGuideStreamingResult(
                ComposeResponse(true, "Potatoes")));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillRepeatedly(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  page_handler()->Compose(user_input, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);

  page_handler()->Rewrite(compose::mojom::StyleModifier::kLonger);
  result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Tomatoes", result->result);
  histograms().ExpectBucketCount(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kLengthElaborateRequest, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kLengthElaborateRequest, 1);

  page_handler()->Rewrite(compose::mojom::StyleModifier::kShorter);
  result = test_future.Take();
  histograms().ExpectBucketCount(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kLengthShortenRequest, 1);
  histograms().ExpectBucketCount(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kLengthShortenRequest, 1);

  // Make sure the async call to CloseUI completes before navigating away.
  FlushMojo();

  // Navigate page away to upload UKM metrics to the collector.
  NavigateAndCommitActiveTab(GURL("about:blank"));

  // Check Compose Session Event Counts.
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kFormalClicked, 0);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kCasualClicked, 0);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kElaborateClicked, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kShortenClicked, 1);
  histograms().ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kAnyModifierUsed, 1);

  // Check session level UKM metrics.
  auto session_ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_SessionProgress::kEntryName,
      {ukm::builders::Compose_SessionProgress::kLengthenCountName,
       ukm::builders::Compose_SessionProgress::kShortenCountName});

  EXPECT_EQ(session_ukm_entries.size(), 1UL);

  EXPECT_THAT(
      session_ukm_entries[0].metrics,
      testing::UnorderedElementsAre(
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kLengthenCountName, 1),
          testing::Pair(
              ukm::builders::Compose_SessionProgress::kShortenCountName, 1)));
}

TEST_F(ChromeComposeClientTest, TestOfflineError) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            std::move(callback).Run(
                OptimizationGuideModelStreamingExecutionResult(
                    base::unexpected(
                        OptimizationGuideModelExecutionError::
                            FromModelExecutionError(
                                optimization_guide::
                                    OptimizationGuideModelExecutionError::
                                        ModelExecutionError::kGenericFailure)),
                    /*provided_by_on_device=*/false,
                    std::make_unique<optimization_guide::ModelQualityLogEntry>(
                        std::make_unique<
                            optimization_guide::proto::LogAiDataRequest>(),
                        nullptr)));
          })));

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce(
          testing::Invoke([&](compose::mojom::ComposeResponsePtr response) {
            test_future.SetValue(std::move(response));
          }));

  // Go offline and then run Compose.
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  page_handler()->Compose("a user typed this", false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kOffline, result->status);
}

TEST_F(ChromeComposeClientTest, TestInnerText) {
  EXPECT_CALL(model_inner_text(), GetInnerText(_, _, _))
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([&](content_extraction::InnerTextCallback callback) {
            std::unique_ptr<content_extraction::InnerTextResult>
                expected_inner_text =
                    std::make_unique<content_extraction::InnerTextResult>(
                        "inner_text", 123);
            std::move(callback).Run(std::move(expected_inner_text));
          })));

  base::test::TestFuture<optimization_guide::proto::ComposeRequest> test_future;
  EXPECT_CALL(session(), AddContext(_))
      .WillOnce(testing::WithArg<0>(testing::Invoke(
          [&](const google::protobuf::MessageLite& request_metadata) {
            optimization_guide::proto::ComposeRequest request;
            request.CheckTypeAndMergeFrom(request_metadata);
            test_future.SetValue(request);
          })));

  ShowDialogAndBindMojo();
  page_handler()->Compose("a user typed this", false);
  optimization_guide::proto::ComposeRequest result = test_future.Take();

  std::string result_string;
  EXPECT_TRUE(result.SerializeToString(&result_string));
  EXPECT_EQ("inner_text", result.page_metadata().page_inner_text());
  EXPECT_EQ(123u, result.page_metadata().page_inner_text_offset());
}

TEST_F(ChromeComposeClientTest, TestInnerTextNodeOffsetNotFound) {
  EXPECT_CALL(model_inner_text(), GetInnerText(_, _, _))
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([&](content_extraction::InnerTextCallback callback) {
            std::unique_ptr<content_extraction::InnerTextResult>
                expected_inner_text =
                    std::make_unique<content_extraction::InnerTextResult>(
                        "inner_text", std::nullopt);
            std::move(callback).Run(std::move(expected_inner_text));
          })));

  base::test::TestFuture<optimization_guide::proto::ComposeRequest> test_future;
  EXPECT_CALL(session(), AddContext(_))
      .WillOnce(testing::WithArg<0>(testing::Invoke(
          [&](const google::protobuf::MessageLite& request_metadata) {
            optimization_guide::proto::ComposeRequest request;
            request.CheckTypeAndMergeFrom(request_metadata);
            test_future.SetValue(request);
          })));

  ShowDialogAndBindMojo();
  page_handler()->Compose("a user typed this", false);
  optimization_guide::proto::ComposeRequest result = test_future.Take();

  std::string result_string;
  EXPECT_TRUE(result.SerializeToString(&result_string));
  EXPECT_EQ("inner_text", result.page_metadata().page_inner_text());
  histograms().ExpectUniqueSample(
      compose::kInnerTextNodeOffsetFound,
      compose::ComposeInnerTextNodeOffset::kNoOffsetFound, 1);
}

TEST_F(ChromeComposeClientTest, TestCloseReasonCanceledWhileWaiting) {
  ShowDialogAndBindMojo();
  EXPECT_CALL(session(), ExecuteModel(_, _))
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            // This is a no-op.
          })));

  page_handler()->Compose("a user typed this", false);

  base::test::TestFuture<compose::mojom::OpenMetadataPtr> open_test_future;
  page_handler()->RequestInitialState(open_test_future.GetCallback());
  compose::mojom::OpenMetadataPtr result = open_test_future.Take();
  EXPECT_TRUE(result->compose_state->has_pending_request);

  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  histograms().ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCanceledBeforeResponseReceived, 1);
}

TEST_F(ChromeComposeClientTest, TestShowNudgeAtCursorFeatureFlag) {
  // Showing nudge at cursor is disabled by default
  EXPECT_FALSE(compose::GetMutableConfigForTesting().is_nudge_shown_at_cursor);

  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{compose::features::kEnableComposeNudgeAtCursor},
      /*disabled_features=*/{});
  // Needed for feature params to apply.
  compose::ResetConfigForTesting();

  EXPECT_TRUE(compose::GetMutableConfigForTesting().is_nudge_shown_at_cursor);
}

#if defined(GTEST_HAS_DEATH_TEST)
// Tests that the Compose client crashes the browser if a webcontents
// tries to bind mojo without opening the dialog at a non Compose URL.
TEST_F(ChromeComposeClientTest, NoStateCrashesAtOtherUrls) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  // We skip showing the dialog here to validate that non special URLs check.
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
// sends any more messages after closing the dialog at
// chrome-untrusted://compose.
TEST_F(ChromeComposeClientTest,
       TestCannotSendMessagesAfterClosingDialogAtChromeCompose) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  NavigateAndCommitActiveTab(GURL(chrome::kChromeUIUntrustedComposeUrl));
  // We skip the dialog showing here, as there is no dialog required at this
  // URL.
  BindMojo();
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
  // Any message after closing the session will crash.
  EXPECT_DEATH(page_handler()->SaveWebUIState(""), "");
}
#endif  // GTEST_HAS_DEATH_TEST
