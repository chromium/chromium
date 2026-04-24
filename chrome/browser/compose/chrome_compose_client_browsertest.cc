// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/compose/compose_session.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/common/compose/compose.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::NiceMock;
using ComposeCallback = ::base::OnceCallback<void(const std::u16string&)>;
using ::optimization_guide::OptimizationGuideModelExecutionError;
using ::optimization_guide::OptimizationGuideModelExecutionResult;
using ::optimization_guide::TestModelQualityLogsUploaderService;
using ::optimization_guide::proto::ModelExecutionInfo;
using ::segmentation_platform::MockSegmentationPlatformService;

const uint64_t kSessionIdHigh = 1234;
const uint64_t kSessionIdLow = 5678;
const segmentation_platform::TrainingRequestId kTrainingRequestId =
    segmentation_platform::TrainingRequestId(456);

class MockInnerText : public InnerTextProvider {
 public:
  MOCK_METHOD(void,
              GetInnerText,
              (content::RenderFrameHost & host,
               std::optional<int> node_id,
               content_extraction::InnerTextCallback callback));
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

std::unique_ptr<KeyedService> BuildMockSegmentationPlatformService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockSegmentationPlatformService>>();
}

std::unique_ptr<KeyedService> BuildMockOptimizationGuideKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
}

}  // namespace

class ChromeComposeClientBrowserTest : public InProcessBrowserTest {
 public:
  ChromeComposeClientBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {compose::features::kEnableCompose,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ChromeComposeClientBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    segmentation_platform::SegmentationPlatformServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(&BuildMockSegmentationPlatformService));
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockOptimizationGuideKeyedService));
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockHatsService));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    scoped_compose_enabled_ = ComposeEnabling::ScopedEnableComposeForTesting();

    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetForProfile(browser()->profile(), true));
    EXPECT_CALL(*mock_hats_service_, CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));

    compose::ResetConfigForTesting();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    GetOptimizationGuide().SetModelQualityLogsUploaderServiceForTesting(
        std::make_unique<TestModelQualityLogsUploaderService>(
            g_browser_process->local_state()));

    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPrefHasCompletedComposeFRE, true);
    SetPrefsForComposeMSBBState(true);

    ON_CALL(
        GetOptimizationGuide(),
        CanApplyOptimization(
            testing::_, optimization_guide::proto::OptimizationType::COMPOSE,
            testing::An<optimization_guide::OptimizationMetadata*>()))
        .WillByDefault(
            [](const GURL& url,
               optimization_guide::proto::OptimizationType optimization_type,
               optimization_guide::OptimizationMetadata* metadata)
                -> optimization_guide::OptimizationGuideDecision {
              *metadata = {};
              compose::ComposeHintMetadata compose_hint_metadata;
              compose_hint_metadata.set_decision(
                  compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED);
              metadata->set_any_metadata(
                  optimization_guide::AnyWrapProto(compose_hint_metadata));
              return optimization_guide::OptimizationGuideDecision::kTrue;
            });

    // Set up embedded test server for valid cross-origin or local navs
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Navigate to a real page with a textarea for autofill/compose triggers.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("foo.com", "/empty.html")));

    // Inject a textarea
    ASSERT_TRUE(content::ExecJs(web_contents(),
                                "document.body.innerHTML = '<textarea "
                                "id=\"test_field\"></textarea>';"));

    client_ = ChromeComposeClient::FromWebContents(web_contents());
    client_->SetModelExecutorForTest(&model_executor_);
    client_->SetModelQualityLogsUploaderServiceForTest(
        GetOptimizationGuide().GetModelQualityLogsUploaderService());
    client_->SetInnerTextProviderForTest(&model_inner_text_);
    client_->SetSkipShowDialogForTest(true);
    client_->SetSessionIdForTest(base::Token(kSessionIdHigh, kSessionIdLow));

    ON_CALL(model_inner_text_, GetInnerText(_, _, _))
        .WillByDefault(testing::WithArg<2>(
            [&](content_extraction::InnerTextCallback callback) {
              std::unique_ptr<content_extraction::InnerTextResult>
                  expected_inner_text =
                      std::make_unique<content_extraction::InnerTextResult>("",
                                                                            0);
              std::move(callback).Run(std::move(expected_inner_text));
            }));
  }

  void TearDownOnMainThread() override {
    client_page_handler_.reset();
    page_handler_.reset();
    callback_router_.reset();

    mock_hats_service_ = nullptr;
    testing::Mock::VerifyAndClear(&GetSegmentationPlatformService());
    client_ = nullptr;
    ukm_recorder_.reset();
    compose::ResetConfigForTesting();

    InProcessBrowserTest::TearDownOnMainThread();
  }

  ChromeComposeClient& client() { return *client_; }

  void SetSkipSuggestionTypeForTest(bool skip_suggestion_type) {
    client_->field_change_observer_.SetSkipSuggestionTypeForTest(
        skip_suggestion_type);
  }

  unsigned int text_field_value_change_event_count() {
    return client_->field_change_observer_.text_field_value_change_event_count_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void InsertText() {
    std::string js =
        "var el = document.getElementById('test_field');"
        "el.focus();"
        "document.execCommand('insertText', false, 'a');";
    ASSERT_TRUE(content::ExecJs(web_contents(), js));
  }

  // Focuses the textarea with id 'test_field' in the DOM.
  void FocusField() {
    ASSERT_TRUE(content::ExecJs(
        web_contents(), "document.getElementById('test_field').focus();"));
  }

  void SetPrefsForComposeMSBBState(bool msbb_state) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        msbb_state);
  }

  // Sets up mock model execution. `times` specifies the expected number of
  // calls. `response_output` is the output string for successful execution.
  // If `response_output` is nullopt, an empty Any object is returned.
  // `success` specifies whether the execution should succeed or fail with a
  // generic error.
  void SetupMockModelExecution(
      int times = 1,
      std::optional<std::string> response_output = "Cucumbers",
      bool success = true) {
    auto action = testing::WithArg<3>(
        [response_output, success](
            optimization_guide::OptimizationGuideModelExecutionResultCallback
                callback) {
          if (success) {
            optimization_guide::proto::Any any_response;
            if (response_output.has_value()) {
              optimization_guide::proto::ComposeResponse response;
              response.set_output(*response_output);
              any_response = optimization_guide::AnyWrapProto(response);
            }
            std::move(callback).Run(
                OptimizationGuideModelExecutionResult(
                    base::ok(any_response),
                    std::make_unique<
                        optimization_guide::proto::ModelExecutionInfo>()),
                /*model_quality_log_entry=*/nullptr);
          } else {
            std::move(callback).Run(
                OptimizationGuideModelExecutionResult(
                    base::unexpected(
                        OptimizationGuideModelExecutionError::
                            FromModelExecutionError(
                                OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure)),
                    std::make_unique<
                        optimization_guide::proto::ModelExecutionInfo>()),
                /*model_quality_log_entry=*/nullptr);
          }
        });

    EXPECT_CALL(model_executor(),
                ExecuteModel(testing::_, testing::_, testing::_, testing::_))
        .Times(times)
        .WillRepeatedly(std::move(action));
  }

  void SetupMockSegmentationResult(
      const std::string& label =
          segmentation_platform::kComposePrmotionLabelShow) {
    EXPECT_CALL(
        GetSegmentationPlatformService(),
        GetClassificationResult(testing::_, testing::_, testing::_, testing::_))
        .WillRepeatedly(testing::WithArg<3>(
            [label](
                segmentation_platform::ClassificationResultCallback callback) {
              auto result = segmentation_platform::ClassificationResult(
                  segmentation_platform::PredictionStatus::kSucceeded);
              result.request_id = kTrainingRequestId;
              result.ordered_labels = {label};
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), result));
            }));
  }

  MockOptimizationGuideKeyedService& GetOptimizationGuide() {
    return *static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile()));
  }

  optimization_guide::TestModelQualityLogsUploaderService& logs_uploader() {
    return *static_cast<
        optimization_guide::TestModelQualityLogsUploaderService*>(
        GetOptimizationGuide().GetModelQualityLogsUploaderService());
  }

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
  uploaded_logs() {
    return logs_uploader().uploaded_logs();
  }

  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return *ukm_recorder_; }

  MockSegmentationPlatformService& GetSegmentationPlatformService() {
    return *static_cast<MockSegmentationPlatformService*>(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetForProfile(browser()->profile()));
  }

  void ShowDialogAndBindMojo(ComposeCallback callback = base::NullCallback()) {
    ShowDialogAndBindMojoWithFieldData(field_data(), std::move(callback));
  }

  void ShowDialogAndBindMojoWithFieldData(
      autofill::FormFieldData field_data,
      ComposeCallback callback = base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint entry_point =
          autofill::AutofillComposeDelegate::UiEntryPoint::kContextMenu) {
    client().ShowComposeDialog(entry_point, field_data, std::move(callback));

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
        &compose_dialog_);
    mojo::PendingRemote<compose::mojom::ComposeUntrustedDialog>
        callback_router_pending_remote =
            callback_router_->BindNewPipeAndPassRemote();

    // Bind mojo to client.
    client_->BindComposeDialog(std::move(client_page_handler_pending_receiver),
                               std::move(page_handler_pending_receiver),
                               std::move(callback_router_pending_remote));
  }

  void BindComposeFutureToOnResponseReceived(
      base::test::TestFuture<compose::mojom::ComposeResponsePtr>&
          compose_future) {
    ON_CALL(compose_dialog_, ResponseReceived(_))
        .WillByDefault([&](compose::mojom::ComposeResponsePtr response) {
          compose_future.SetValue(std::move(response));
        });
  }

  autofill::FormFieldData field_data() {
    autofill::FormFieldData field;
    field.set_name(u"test_field");
    field.set_id_attribute(u"test_field");
    return field;
  }

  void FlushPageHandler() { page_handler_.FlushForTesting(); }

  ComposeSession* GetSessionForActiveComposeField() {
    return client().GetSessionForActiveComposeField();
  }

  compose::mojom::ComposeSessionUntrustedPageHandler* page_handler() {
    return page_handler_.get();
  }

  mojo::Remote<compose::mojom::ComposeClientUntrustedPageHandler>&
  client_page_handler() {
    return client_page_handler_;
  }

  MockComposeDialog& compose_dialog() { return compose_dialog_; }

  testing::NiceMock<optimization_guide::MockRemoteModelExecutor>&
  model_executor() {
    return model_executor_;
  }

  // Creates a simple FormData with a single textarea field.
  // We construct this manually instead of using
  // `autofill::test::CreateTestFormField` because that helper requires
  // `autofill::test::AutofillBrowserTestEnvironment`.
  // Instantiating `AutofillBrowserTestEnvironment` in a browser test
  // introduces a conflicting `ScopedFeatureList` that crashes the test
  // during teardown.
  autofill::FormData CreateManualFormData(const GURL& url = GURL()) {
    autofill::FormData form_data;
    if (url.is_empty()) {
      form_data.set_url(browser()
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetPrimaryMainFrame()
                            ->GetLastCommittedURL());
    } else {
      form_data.set_url(url);
    }
    autofill::FormFieldData field;
    field.set_name(u"name0");
    field.set_id_attribute(u"name0");
    field.set_value(u"value0");
    field.set_form_control_type(autofill::FormControlType::kTextArea);
    form_data.set_fields({field});
    return form_data;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;

 private:
  raw_ptr<ChromeComposeClient> client_;
  testing::NiceMock<optimization_guide::MockRemoteModelExecutor>
      model_executor_;
  testing::NiceMock<MockInnerText> model_inner_text_;
  testing::NiceMock<MockComposeDialog> compose_dialog_;
  std::unique_ptr<mojo::Receiver<compose::mojom::ComposeUntrustedDialog>>
      callback_router_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  mojo::Remote<compose::mojom::ComposeClientUntrustedPageHandler>
      client_page_handler_;
  mojo::Remote<compose::mojom::ComposeSessionUntrustedPageHandler>
      page_handler_;
  ComposeEnabling::ScopedOverride scoped_compose_enabled_;
  raw_ptr<MockHatsService> mock_hats_service_;
};

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TextFieldChangeThresholdHidesProactiveNudge) {
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_show_probability = 1.0;
  config.proactive_nudge_segmentation = false;

  SetSkipSuggestionTypeForTest(true);

  // Instead of using an Autofill test environment to mock forms,
  // we trigger the autofill client popup natively from the WebContents.
  autofill::ChromeAutofillClient* autofill_client =
      autofill::ChromeAutofillClient::FromWebContentsForTesting(web_contents());

  autofill::AutofillClient::PopupOpenArgs args;
  args.suggestions = {
      autofill::Suggestion(autofill::SuggestionType::kComposeProactiveNudge)};
  autofill_client->ShowAutofillSuggestions(args, /*delegate=*/nullptr);

  // Simulate field change events by actually dispatching input events natively
  // on the textarea, and wait for the IPC to process the text change.
  unsigned int max = config.nudge_field_change_event_max;
  for (size_t i = 1; i < max; i++) {
    InsertText();

    // We must wait for the text change IPC to reach the browser process.
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return text_field_value_change_event_count() == i; }));
  }

  // Reaching the event threshold resets the event count and hides the Autofill
  // popup natively.
  InsertText();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return text_field_value_change_event_count() == 0U; }));
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       InputModeUnsetHistogramTest) {
  base::HistogramTester histograms;

  SetupMockModelExecution();
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);

  page_handler()->Compose("", compose::mojom::InputMode::kUnset, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();

  histograms.ExpectUniqueSample(compose::kComposeRequestReason,
                                compose::ComposeRequestReason::kFirstRequest,
                                1);
  histograms.ExpectUniqueSample("Compose.Server.Request.Reason",
                                compose::ComposeRequestReason::kFirstRequest,
                                1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       InputModePolishHistogramTest) {
  base::HistogramTester histograms;

  SetupMockModelExecution();
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);

  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();

  histograms.ExpectUniqueSample(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kFirstRequestPolishMode, 1);
  histograms.ExpectUniqueSample(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kFirstRequestPolishMode, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       InputModeElaborateHistogramTest) {
  base::HistogramTester histograms;

  SetupMockModelExecution();
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);

  page_handler()->Compose("", compose::mojom::InputMode::kElaborate, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();

  histograms.ExpectUniqueSample(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kFirstRequestElaborateMode, 1);
  histograms.ExpectUniqueSample(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kFirstRequestElaborateMode, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       InputModeFormalizeHistogramTest) {
  base::HistogramTester histograms;

  SetupMockModelExecution();
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);

  page_handler()->Compose("", compose::mojom::InputMode::kFormalize, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();

  histograms.ExpectUniqueSample(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kFirstRequestFormalizeMode, 1);
  histograms.ExpectUniqueSample(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kFirstRequestFormalizeMode, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       CloseButtonHistogramTest) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;
  base::ScopedMockElapsedTimersForTest test_timer;

  SetupMockModelExecution(3);

  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  // Simulate three Compose requests - two from edits.
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);
  compose::mojom::ComposeResponsePtr response = compose_future.Take();

  page_handler()->Compose("", compose::mojom::InputMode::kPolish, true);
  response = compose_future.Take();

  page_handler()->Compose("", compose::mojom::InputMode::kPolish, true);
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

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.EndedSession.CloseButtonClicked"));

  // Expect that the close button click was recorded.
  histograms.ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCloseButtonPressed, 1);

  // Expect that three total Compose calls were recorded.
  histograms.ExpectUniqueSample(
      compose::kComposeSessionComposeCount + std::string(".Ignored"), 3, 1);
  histograms.ExpectUniqueSample("Compose.Server.Session.ComposeCount.Ignored",
                                3, 1);

  // Expect that two of the Compose calls were from edits.
  histograms.ExpectUniqueSample(
      compose::kComposeSessionUpdateInputCount + std::string(".Ignored"), 2, 1);
  histograms.ExpectUniqueSample(
      "Compose.Server.Session.SubmitEditCount.Ignored", 2, 1);

  // Expect that two undos were done.
  histograms.ExpectUniqueSample(
      compose::kComposeSessionUndoCount + std::string(".Ignored"), 2, 1);
  histograms.ExpectUniqueSample("Compose.Server.Session.UndoCount.Ignored", 2,
                                1);

  // Expect that the dialog was shown twice.
  histograms.ExpectUniqueSample(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 2, 1);
  histograms.ExpectUniqueSample(
      "Compose.Server.Session.DialogShownCount.Ignored", 2, 1);

  // Check expected session duration metrics
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".FRE"), 0);
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".MSBB"), 0);
  histograms.ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".Ignored"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms.ExpectUniqueTimeSample(
      "Compose.Server.Session.Duration.Ignored",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms.ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);

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
    histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.Server.Session.EventCounts",
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                 event_type, 0);
  }

  // No FRE related close reasons should have been recorded.
  histograms.ExpectTotalCount(compose::kComposeFirstRunSessionCloseReason, 0);

  // No MSBB related close reasons should have been recorded.
  histograms.ExpectTotalCount(compose::kComposeMSBBSessionCloseReason, 0);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       ExpiredSessionHistogramTest) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;
  base::ScopedMockElapsedTimersForTest test_timer;

  compose::Config& config = compose::GetMutableConfigForTesting();
  // ElapsedTimer in test will return an elapsed time of 1337ms by default.
  // Set the session lifetime threshold to be shorter than this to simulate
  // expiry.
  config.session_max_allowed_lifetime = base::Seconds(1);

  ShowDialogAndBindMojo();
  // Show the dialog a second time - this ends the previous session if it is now
  // expired.
  ShowDialogAndBindMojo();

  histograms.ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kExceededMaxDuration, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.EndedSession.EndedImplicitly"));
  // Expect that the dialog was shown once.
  histograms.ExpectUniqueSample(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 1, 1);

  // Check expected session duration metrics
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".FRE"), 0);
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".MSBB"), 0);
  histograms.ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".Ignored"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms.ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);

  // No FRE related close reasons should have been recorded.
  histograms.ExpectTotalCount(compose::kComposeFirstRunSessionCloseReason, 0);
  // No MSBB related close reasons should have been recorded.
  histograms.ExpectTotalCount(compose::kComposeMSBBSessionCloseReason, 0);

  client().CloseUI(compose::mojom::CloseReason::kCloseButton);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       ExpiredSessionMSBBHistogramTest) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;
  base::ScopedMockElapsedTimersForTest test_timer;

  SetPrefsForComposeMSBBState(false);

  compose::Config& config = compose::GetMutableConfigForTesting();
  // ElapsedTimer in test will return an elapsed time of 1337ms by default.
  // Set the session lifetime threshold to be shorter than this to simulate
  // expiry.
  config.session_max_allowed_lifetime = base::Seconds(1);

  ShowDialogAndBindMojo();
  // Show the dialog a second time - this ends the previous session if it is now
  // expired.
  ShowDialogAndBindMojo();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.EndedSession.EndedImplicitly"));

  histograms.ExpectUniqueSample(
      compose::kComposeMSBBSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kExceededMaxDuration, 1);

  histograms.ExpectUniqueSample(
      compose::kComposeMSBBSessionDialogShownCount + std::string(".Ignored"),
      1,  // Expect that one total MSBB dialog was shown.
      1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       ExpiredSessionFirstRunHistogramTest) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;
  base::ScopedMockElapsedTimersForTest test_timer;

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrefHasCompletedComposeFRE, false);

  compose::Config& config = compose::GetMutableConfigForTesting();
  // ElapsedTimer in test will return an elapsed time of 1337ms by default.
  // Set the session lifetime threshold to be shorter than this to simulate
  // expiry.
  config.session_max_allowed_lifetime = base::Seconds(1);

  ShowDialogAndBindMojo();
  // Show the dialog a second time - this ends the previous session if it is now
  // expired.
  ShowDialogAndBindMojo();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.EndedSession.EndedImplicitly"));

  histograms.ExpectUniqueSample(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kExceededMaxDuration, 1);

  histograms.ExpectUniqueSample(
      compose::kComposeFirstRunSessionDialogShownCount +
          std::string(".Ignored"),
      1,  // Expect that one total FRE dialog was shown.
      1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       CloseButtonMSBBHistogramTest) {
  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest test_timer;

  SetPrefsForComposeMSBBState(false);
  ShowDialogAndBindMojo();

  client().CloseUI(compose::mojom::CloseReason::kMSBBCloseButton);

  histograms.ExpectUniqueSample(
      compose::kComposeMSBBSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kCloseButtonPressed, 1);

  histograms.ExpectUniqueSample(
      compose::kComposeMSBBSessionDialogShownCount + std::string(".Ignored"),
      1,  // Expect that one total MSBB dialog was shown.
      1);
  histograms.ExpectTotalCount(compose::kComposeMSBBSessionCloseReason, 1);

  // No FRE related close reasons should have been recorded.
  histograms.ExpectTotalCount(compose::kComposeFirstRunSessionCloseReason, 0);

  // Check expected session duration metrics
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".FRE"), 0);
  histograms.ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".MSBB"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".Inserted"), 0);
  histograms.ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);

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
    histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.Server.Session.EventCounts",
                                 event_type, 0);
    histograms.ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                 event_type, 0);
  }
}

// Tests that quality logs are uploaded when a new valid response clears forward
// state and when the session is destroyed, and that those logs have the
// expected session IDs attached.
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeQualitySessionId) {
  client().SetSessionIdForTest(base::Token(kSessionIdHigh, kSessionIdLow));

  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution(3);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  page_handler()->Compose("a user typed one",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Wait());
  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed two",
                          compose::mojom::InputMode::kPolish, false);
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
  page_handler()->Compose("a user typed three",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Wait());

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  const auto& session_id = uploaded_logs()[0]->compose().quality().session_id();
  EXPECT_EQ(kSessionIdHigh, session_id.high());
  EXPECT_EQ(kSessionIdLow, session_id.low());

  // Wait for two log uploads.
  log_uploaded_signal.Clear();
  logs_uploader().WaitForLogUpload(
      log_uploaded_signal.GetCallback().Then(base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(log_uploaded_signal.WaitAndClear());
        logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());
      })));

  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(3u, uploaded_logs().size());
  const auto& session_id2 =
      uploaded_logs()[1]->compose().quality().session_id();
  EXPECT_EQ(kSessionIdHigh, session_id2.high());
  EXPECT_EQ(kSessionIdLow, session_id2.low());
  const auto& session_id3 =
      uploaded_logs()[2]->compose().quality().session_id();
  EXPECT_EQ(kSessionIdHigh, session_id3.high());
  EXPECT_EQ(kSessionIdLow, session_id3.low());
  EXPECT_EQ(
      optimization_guide::proto::FinalModelStatus::FINAL_MODEL_STATUS_SUCCESS,
      uploaded_logs()[1]->compose().quality().final_model_status());
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       CloseButtonMSBBEnabledDuringSessionHistogramTest) {
  base::HistogramTester histograms;
  SetPrefsForComposeMSBBState(false);
  ShowDialogAndBindMojo();

  SetPrefsForComposeMSBBState(true);
  // Show the dialog a second time.
  ShowDialogAndBindMojo();

  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  histograms.ExpectUniqueSample(
      compose::kComposeSessionComposeCount + std::string(".Ignored"),
      0,  // Expect that zero total Compose calls were recorded.
      1);

  histograms.ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCloseButtonPressed, 1);

  histograms.ExpectUniqueSample(compose::kComposeMSBBSessionCloseReason,
                                compose::ComposeFreOrMsbbSessionCloseReason::
                                    kAckedOrAcceptedWithoutInsert,
                                1);

  histograms.ExpectUniqueSample(
      compose::kComposeMSBBSessionDialogShownCount + std::string(".Accepted"),
      1,  // Expect that the dialog was shown once.
      1);
  histograms.ExpectTotalCount(compose::kComposeMSBBSessionCloseReason, 1);

  // No FRE related close reasons should have been recorded.
  histograms.ExpectTotalCount(compose::kComposeFirstRunSessionCloseReason, 0);

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
    histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.Server.Session.EventCounts",
                                 event_type, 0);
    histograms.ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                 event_type, 0);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       MSBBCloseDialogHistogramTest) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;

  // Set MSBB dialog state to show.
  SetPrefsForComposeMSBBState(false);
  // Dialog should show at MSBB state (and not first run).
  ShowDialogAndBindMojo();
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Compose.DialogSeen.FirstRunMSBB"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Compose.DialogSeen.FirstRunDisclaimer"));

  // End the session by re-opening with selection.
  autofill::FormFieldData field = field_data();
  field.set_value(u"user selected text");
  field.set_selected_text(u"selected text");
  ShowDialogAndBindMojoWithFieldData(field);

  histograms.ExpectUniqueSample(
      compose::kComposeMSBBSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kReplacedWithNewSession, 1);
  histograms.ExpectTotalCount(compose::kComposeFirstRunSessionCloseReason, 0);
  // Expect that the MSBB dialog was shown+ignored once.
  histograms.ExpectBucketCount(
      compose::kComposeMSBBSessionDialogShownCount + std::string(".Ignored"), 1,
      1);

  // The main dialog close reason should be |kEndedAtMsbb|.
  histograms.ExpectTotalCount(compose::kComposeSessionCloseReason, 1);
  histograms.ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kEndedAtMsbb, 1);

  // The main dialog should not be shown.
  histograms.ExpectTotalCount(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 0);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       AcceptSuggestionHistogramTest) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;
  base::ScopedMockElapsedTimersForTest test_timer;

  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution(3);

  // Simulate three compose requests - two from edits.
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);
  compose::mojom::ComposeResponsePtr response = compose_future.Take();

  page_handler()->Compose("", compose::mojom::InputMode::kPolish, true);
  response = compose_future.Take();

  page_handler()->Compose("", compose::mojom::InputMode::kPolish, true);
  response = compose_future.Take();

  // Show the dialog a second time.
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  compose::mojom::ComposeStatePtr state = undo_future.Take();

  // Show the dialog a third time.
  ShowDialogAndBindMojo();

  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.EndedSession.InsertButtonClicked"));
  histograms.ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kInsertedResponse, 1);
  histograms.ExpectUniqueSample(
      compose::kComposeSessionComposeCount + std::string(".Accepted"),
      3,  // Expect that three Compose calls were recorded.
      1);
  histograms.ExpectUniqueSample(
      compose::kComposeSessionUpdateInputCount + std::string(".Accepted"),
      2,  // Expect that two of the Compose calls were from edits.
      1);
  histograms.ExpectUniqueSample(
      compose::kComposeSessionUndoCount + std::string(".Accepted"),
      1,  // Expect that one undo was done.
      1);
  histograms.ExpectUniqueSample(
      compose::kComposeSessionDialogShownCount + std::string(".Accepted"), 3,
      1);
  histograms.ExpectUniqueSample(
      "Compose.Server.Session.DialogShownCount.Accepted", 3, 1);

  // Check expected session duration metrics.
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".FRE"), 0);
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".MSBB"), 0);
  histograms.ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".Inserted"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms.ExpectUniqueTimeSample(
      "Compose.Server.Session.Duration.Inserted",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms.ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       ComposeDialogStatesSeenUserActionsTest) {
  base::UserActionTester user_action_tester;
  // Set both FRE and MSBB dialog states to show and check that appropriate
  // user actions are logged when moving through all states in a single session.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  SetPrefsForComposeMSBBState(false);
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Compose.DialogSeen.FirstRunDisclaimer"));
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Compose.DialogSeen.FirstRunMSBB"));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("Compose.DialogSeen.MainDialog"));

  // Dialog should show at FRE state.
  ShowDialogAndBindMojo();
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.DialogSeen.FirstRunDisclaimer"));
  // After acknowledging the disclaimer, dialog should show the MSBB state.
  client().CompleteFirstRun();
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Compose.DialogSeen.FirstRunMSBB"));
  // After updating the MSBB setting, only the next open of the dialog should
  // record  a dialog seen action.
  SetPrefsForComposeMSBBState(true);
  ShowDialogAndBindMojo();
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Compose.DialogSeen.MainDialog"));
  // Show dialog again.
  ShowDialogAndBindMojo();
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Compose.DialogSeen.MainDialog"));
  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Check user actions for new session opened at MSBB state.
  SetPrefsForComposeMSBBState(false);
  ShowDialogAndBindMojo();
  EXPECT_EQ(
      2, user_action_tester.GetActionCount("Compose.DialogSeen.FirstRunMSBB"));
  client().CloseUI(compose::mojom::CloseReason::kMSBBCloseButton);

  // Check user actions for new session opened at main dialog state.
  SetPrefsForComposeMSBBState(true);
  ShowDialogAndBindMojo();
  EXPECT_EQ(2,
            user_action_tester.GetActionCount("Compose.DialogSeen.MainDialog"));
  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Check user actions for session opened at FRE state and progressing directly
  // to main dialog state.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  ShowDialogAndBindMojo();
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Compose.DialogSeen.FirstRunDisclaimer"));
  // After acknowledging the disclaimer, dialog should show the main state.
  client().CompleteFirstRun();
  EXPECT_EQ(3,
            user_action_tester.GetActionCount("Compose.DialogSeen.MainDialog"));
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       FirstRunCloseDialogHistogramTest) {
  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest test_timer;

  // Enable FRE and show the dialog.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrefHasCompletedComposeFRE, false);
  ShowDialogAndBindMojo();
  client().CloseUI(compose::mojom::CloseReason::kFirstRunCloseButton);

  // The FRE close reason should be |kCloseButtonPressed|.
  histograms.ExpectUniqueSample(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kCloseButtonPressed, 1);
  // The main dialog close reason should be |kEndedAtFre|.
  histograms.ExpectTotalCount(compose::kComposeSessionCloseReason, 1);
  histograms.ExpectUniqueSample(compose::kComposeSessionCloseReason,
                                compose::ComposeSessionCloseReason::kEndedAtFre,
                                1);
  // Expect that the dialog was shown once ending without FRE completed.
  histograms.ExpectUniqueSample(
      compose::kComposeFirstRunSessionDialogShownCount +
          std::string(".Ignored"),
      1, 1);

  // Check expected session duration metrics.
  histograms.ExpectUniqueTimeSample(
      compose::kComposeSessionDuration + std::string(".FRE"),
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".MSBB"), 0);
  histograms.ExpectTotalCount(
      compose::kComposeSessionDuration + std::string(".Ignored"), 0);
  histograms.ExpectTotalCount("Compose.Server.Session.Duration.Ignored", 0);
  histograms.ExpectUniqueSample(compose::kComposeSessionOverOneDay, 0, 1);

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
    histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.Server.Session.EventCounts",
                                 event_type, 0);
    histograms.ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                 event_type, 0);
  }

  // Show the FRE dialog and end the session by re-opening with selection.
  ShowDialogAndBindMojo();
  autofill::FormFieldData field = field_data();
  field.set_value(u"user selected text");
  field.set_selected_text(u"selected text");
  ShowDialogAndBindMojoWithFieldData(field);
  histograms.ExpectBucketCount(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kReplacedWithNewSession, 1);
  histograms.ExpectBucketCount(
      compose::kComposeFirstRunSessionDialogShownCount +
          std::string(".Ignored"),
      1,  // Expect that the dialog was shown once.
      2);

  // The main dialog should not be shown.
  histograms.ExpectTotalCount(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 0);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       FirstRunThenMSBBCloseDialogHistogramTest) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;

  // Set both FRE and MSBB dialog states.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrefHasCompletedComposeFRE, false);
  SetPrefsForComposeMSBBState(false);
  // Dialog should show at FRE state.
  ShowDialogAndBindMojo();
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.DialogSeen.FirstRunDisclaimer"));
  // After acknowledging the disclaimer, dialog should show the MSBB state.
  client().CompleteFirstRun();
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Compose.DialogSeen.FirstRunMSBB"));

  // End the session by re-opening with selection.
  autofill::FormFieldData field = field_data();
  field.set_value(u"user selected text");
  field.set_selected_text(u"selected text");
  ShowDialogAndBindMojoWithFieldData(field);

  histograms.ExpectUniqueSample(
      compose::kComposeMSBBSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kReplacedWithNewSession, 1);
  histograms.ExpectBucketCount(compose::kComposeFirstRunSessionCloseReason,
                               compose::ComposeFreOrMsbbSessionCloseReason::
                                   kAckedOrAcceptedWithoutInsert,
                               1);
  // Expect that the FRE dialog was shown+acked once.
  histograms.ExpectBucketCount(
      compose::kComposeFirstRunSessionDialogShownCount +
          std::string(".Acknowledged"),
      1, 1);
  // Expect that the MSBB dialog was shown+ignored once.
  histograms.ExpectBucketCount(
      compose::kComposeMSBBSessionDialogShownCount + std::string(".Ignored"), 1,
      1);

  // The main dialog close reason should be |kAckedFreEndedAtMsbb|.
  histograms.ExpectTotalCount(compose::kComposeSessionCloseReason, 1);
  histograms.ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kAckedFreEndedAtMsbb, 1);

  // The main dialog should not be shown.
  histograms.ExpectTotalCount(
      compose::kComposeSessionDialogShownCount + std::string(".Ignored"), 0);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       FirstRunCompletedHistogramTest) {
  base::HistogramTester histograms;

  // Enable FRE and show the dialog.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrefHasCompletedComposeFRE, false);
  ShowDialogAndBindMojo();
  // Show the dialog a second time.
  ShowDialogAndBindMojo();
  // Complete FRE and close.
  client().CompleteFirstRun();
  client().CloseUI(compose::mojom::CloseReason::kCloseButton);

  histograms.ExpectUniqueSample(compose::kComposeFirstRunSessionCloseReason,
                                compose::ComposeFreOrMsbbSessionCloseReason::
                                    kAckedOrAcceptedWithoutInsert,
                                1);
  // Expect that the dialog was shown twice ending with FRE completed.
  histograms.ExpectUniqueSample(
      compose::kComposeFirstRunSessionDialogShownCount +
          std::string(".Acknowledged"),
      2, 1);

  // After FRE is completed, a new set of metrics should be collected for the
  // remainder of the session.
  histograms.ExpectUniqueSample(
      compose::kComposeSessionCloseReason,
      compose::ComposeSessionCloseReason::kCloseButtonPressed, 1);
  histograms.ExpectUniqueSample(
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
    histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.Server.Session.EventCounts",
                                 event_type, 0);
    histograms.ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                 event_type, 0);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       FirstRunCompletedThenSuggestionAcceptedHistogramTest) {
  base::HistogramTester histograms;

  // Enable FRE and show the dialog.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrefHasCompletedComposeFRE, false);
  ShowDialogAndBindMojo();
  // Complete FRE then close by inserting.
  client().CompleteFirstRun();
  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  histograms.ExpectUniqueSample(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kAckedOrAcceptedWithInsert,
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
    histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.Server.Session.EventCounts",
                                 event_type, 0);
    histograms.ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                 event_type, 0);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest, LoseFocusHistogramTest) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;
  ShowDialogAndBindMojo();

  // Dismiss dialog by losing focus by navigating.
  GURL next_page("http://example.com/a.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), next_page));

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.EndedSession.EndedImplicitly"));
  histograms.ExpectUniqueSample(compose::kComposeSessionCloseReason,
                                compose::ComposeSessionCloseReason::kAbandoned,
                                1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       LoseFocusFirstRunHistogramTest) {
  base::HistogramTester histograms;
  // Enable FRE and show the dialog.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kPrefHasCompletedComposeFRE,
                                       false);
  ShowDialogAndBindMojo();

  // Dismiss dialog by losing focus by navigating.
  GURL next_page("http://example.com/a.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), next_page));

  histograms.ExpectUniqueSample(
      compose::kComposeFirstRunSessionCloseReason,
      compose::ComposeFreOrMsbbSessionCloseReason::kAbandoned, 1);
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestComposeGenericServerError \
  DISABLED_TestComposeGenericServerError
#else
#define MAYBE_TestComposeGenericServerError TestComposeGenericServerError
#endif
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       MAYBE_TestComposeGenericServerError) {
  base::HistogramTester histograms;
  ShowDialogAndBindMojo();
  SetupMockModelExecution(1, "", false);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kServerError, result->status);

  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  EXPECT_TRUE(log_uploaded_signal.Wait());

  // Check that the quality modeling log is still correct
  ASSERT_EQ(1u, uploaded_logs().size());
  const auto& session_id = uploaded_logs()[0]->compose().quality().session_id();
  EXPECT_EQ(kSessionIdHigh, session_id.high());
  EXPECT_EQ(kSessionIdLow, session_id.low());

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
    histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.Server.Session.EventCounts",
                                 event_type, count);
    histograms.ExpectBucketCount("Compose.OnDevice.Session.EventCounts",
                                 event_type, 0);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestContextMenuNotRecordedAsProactiveInQualityLogs) {
  autofill::FormFieldData field = field_data();
  field.set_value(u"user selected text");
  ShowDialogAndBindMojoWithFieldData(
      field, base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kContextMenu);

  SetupMockModelExecution();

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();

  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  EXPECT_FALSE(
      uploaded_logs()[0]->compose().quality().started_with_proactive_nudge());

  // Force reporting of page events UKM.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // NavigateToURL is a blocking call that forces the page to unload and flushes
  // pending UKM events, so we can read them immediately.

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

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestProactiveNudgeRecordedInQualityLogs) {
  autofill::FormFieldData field = field_data();
  field.set_value(u"user selected text");
  ShowDialogAndBindMojoWithFieldData(
      field, base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);

  SetupMockModelExecution();

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);
  compose::mojom::ComposeResponsePtr result = test_future.Take();

  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  EXPECT_TRUE(
      uploaded_logs()[0]->compose().quality().started_with_proactive_nudge());

  // Force reporting of page events UKM.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // NavigateToURL is a blocking call that forces the page to unload and flushes
  // pending UKM events, so we can read them immediately.

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

// Failing consistently on CrOS. crbug.com/503432696
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestComposeQualityLoggedOnSubsequentError \
  DISABLED_TestComposeQualityLoggedOnSubsequentError
#else
#define MAYBE_TestComposeQualityLoggedOnSubsequentError \
  TestComposeQualityLoggedOnSubsequentError
#endif
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       MAYBE_TestComposeQualityLoggedOnSubsequentError) {
  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest test_timer;
  ShowDialogAndBindMojo();
  SetupMockModelExecution(2, "", false);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);

  compose::mojom::ComposeResponsePtr compose_result = compose_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kServerError,
            compose_result->status);

  page_handler()->Compose("a user typed that",
                          compose::mojom::InputMode::kPolish, false);

  compose_result = compose_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kServerError,
            compose_result->status);

  // Ensure that a quality log is emitted after a second compose error.
  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  EXPECT_EQ(kSessionIdLow,
            uploaded_logs()[0]->compose().quality().session_id().low());

  // Close UI to submit remaining quality logs.
  log_uploaded_signal.Clear();
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(2u, uploaded_logs().size());
  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      uploaded_logs()[1]->compose().quality().request_latency_ms());

  // Check that histogram was sent for Compose State removed from undo stack.
  histograms.ExpectBucketCount("Compose.Server.Request.Feedback",
                               compose::ComposeRequestFeedback::kNoFeedback, 0);
  histograms.ExpectBucketCount("Compose.Server.Request.Feedback",
                               compose::ComposeRequestFeedback::kRequestError,
                               2);
}

// Tests that quality logs are uploaded when a new valid response clears forward
// state and when the session is destroyed, and that those logs have expected
// latency data attached.
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeQualityLatency) {
  base::ScopedMockElapsedTimersForTest test_timer;
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution(3);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  page_handler()->Compose("a user typed one",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Wait());
  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed two",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Wait());
  // Reset future for third compose call.
  compose_future.Clear();

  // Undo reverts client to the first saved state in the history, with one
  // forward state resulting from the second compose.
  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  EXPECT_TRUE(undo_future.Wait());

  // Third compose should clear the forward state from the second compose and
  // upload its corresponding quality logs.
  page_handler()->Compose("a user typed three",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Wait());

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      uploaded_logs()[0]->compose().quality().request_latency_ms());

  // Close UI should result in upload of quality logs for the two responses left
  // in the state history.
  log_uploaded_signal.Clear();
  logs_uploader().WaitForLogUpload(
      log_uploaded_signal.GetCallback().Then(base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(log_uploaded_signal.WaitAndClear());
        logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());
      })));

  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(3u, uploaded_logs().size());
  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      uploaded_logs()[1]->compose().quality().request_latency_ms());
  EXPECT_EQ(
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      uploaded_logs()[2]->compose().quality().request_latency_ms());
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest, TestCompose) {
  base::HistogramTester histograms;
  base::UserActionTester user_action_tester;

  SetupMockModelExecution();

  // Simulate page showing context menu.
  auto* rfh = web_contents()->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_content_editable_for_autofill = true;
  params.frame_origin = rfh->GetMainFrame()->GetLastCommittedOrigin();
  EXPECT_TRUE(client().ShouldTriggerContextMenu(rfh, params));

  // Then simulate clicking the dialog.
  ShowDialogAndBindMojo();

  // Now call Compose, checking the results.
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  BindComposeFutureToOnResponseReceived(test_future);
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();

  EXPECT_EQ(compose::mojom::ComposeStatus::kOk, result->status);
  EXPECT_EQ("Cucumbers", result->result);
  EXPECT_FALSE(result->on_device_evaluation_used);

  // Check that the session entry point histogram is recorded.
  histograms.ExpectUniqueSample(compose::kComposeStartSessionEntryPoint,
                                compose::ComposeEntryPoint::kContextMenu, 1);

  // Check that a user action for the Compose request was emitted.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Compose.ComposeRequest.CreateClicked"));
  histograms.ExpectUniqueSample(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kFirstRequestPolishMode, 1);
  histograms.ExpectUniqueSample(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kFirstRequestPolishMode, 1);
  // Check that a request result OK metric was emitted.
  histograms.ExpectUniqueSample(compose::kComposeRequestStatus,
                                compose::mojom::ComposeStatus::kOk, 1);
  histograms.ExpectUniqueSample("Compose.Server.Request.Status",
                                compose::mojom::ComposeStatus::kOk, 1);

  // Check that a request duration OK metric was emitted.
  histograms.ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationOkSuffix}), 1);
  histograms.ExpectTotalCount(
      base::StrCat(
          {"Compose.Server", compose::kComposeRequestDurationOkSuffix}),
      1);

  // Check that no request duration Error metrics were emitted.
  histograms.ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationErrorSuffix}),
      0);
  histograms.ExpectTotalCount(
      base::StrCat(
          {"Compose.Server", compose::kComposeRequestDurationErrorSuffix}),
      0);
  // Check that the request metadata had a valid node offset.
  histograms.ExpectUniqueSample(
      compose::kInnerTextNodeOffsetFound,
      compose::ComposeInnerTextNodeOffset::kOffsetFound, 1);
  // Simulate insert call from Compose dialog.
  page_handler()->AcceptComposeResult(base::NullCallback());
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kInsertButton);
  client_page_handler().FlushForTesting();

  // Check Compose Session Event Counts.
  histograms.ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms.ExpectBucketCount(
      "Compose.Server.Session.EventCounts",
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms.ExpectBucketCount(
      "Compose.OnDevice.Session.EventCounts",
      compose::ComposeSessionEventTypes::kMainDialogShown, 0);
  histograms.ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
  histograms.ExpectBucketCount(
      "Compose.Server.Session.EventCounts",
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
  histograms.ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kCreateClicked, 1);
  histograms.ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kInsertClicked, 1);

  histograms.ExpectUniqueSample("Compose.Session.EvalLocation",
                                compose::SessionEvalLocation::kServer, 1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

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

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestProactiveNudgeEngagementIsRecorded) {
  // Enable and trigger the proactive nudge.
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_show_probability = 1.0;
  config.proactive_nudge_focus_delay = base::Microseconds(4);
  config.proactive_nudge_segmentation = true;
  config.proactive_nudge_always_collect_training_data = true;

  // Mock segmentation platform to allow proactive nudge.
  SetupMockSegmentationResult();

  // Focus the field in the DOM to make it the active element.
  FocusField();

  autofill::FormFieldData field = field_data();
  field.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  field.set_value(u"value0");
  field.set_form_control_type(autofill::FormControlType::kTextArea);
  field.set_allows_writing_suggestions(true);

  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.set_fields({field});

  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldValueChanged;

  // Should trigger after delay (using RunUntil to wait for timer/tasks).
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return client().ShouldTriggerPopup(form_data, field, trigger_source);
  }));

  // Simulate clicking on the nudge to open compose.
  ShowDialogAndBindMojoWithFieldData(
      field, base::NullCallback(),
      autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);

  base::test::TestFuture<segmentation_platform::TrainingLabels> training_labels;
  EXPECT_CALL(GetSegmentationPlatformService(),
              CollectTrainingData(
                  segmentation_platform::proto::SegmentId::
                      OPTIMIZATION_TARGET_SEGMENTATION_COMPOSE_PROMOTION,
                  kTrainingRequestId, testing::_, testing::_, testing::_))
      .WillRepeatedly(testing::WithArg<3>(
          [&](segmentation_platform::TrainingLabels labels) {
            if (labels.output_metric.has_value() &&
                labels.output_metric->second ==
                    static_cast<base::HistogramBase::Sample32>(
                        compose::ProactiveNudgeDerivedEngagement::
                            kAcceptedComposeSuggestion)) {
              if (!training_labels.IsReady()) {
                training_labels.SetValue(labels);
              }
            }
          }));

  client().CloseUI(compose::mojom::CloseReason::kInsertButton);

  // Trigger session deletion and verify that the engagement is recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  EXPECT_EQ(training_labels.Get().output_metric,
            std::make_pair("Compose.ProactiveNudge.DerivedEngagement",
                           static_cast<base::HistogramBase::Sample32>(
                               compose::ProactiveNudgeDerivedEngagement::
                                   kAcceptedComposeSuggestion)));
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestShouldTriggerProactiveNudgeBlockedBySegmentation) {
  base::HistogramTester histograms;

  // Enable and trigger the proactive nudge.
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_show_probability = 1.0;
  config.proactive_nudge_focus_delay = base::Microseconds(4);
  config.proactive_nudge_segmentation = true;

  // Focus the field in the DOM to make it the active element.
  FocusField();

  autofill::FormFieldData field = field_data();
  field.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  field.set_value(u"value0");
  field.set_form_control_type(autofill::FormControlType::kTextArea);
  field.set_allows_writing_suggestions(true);

  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.set_fields({field});

  SetupMockSegmentationResult(
      segmentation_platform::kComposePrmotionLabelDontShow);

  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldValueChanged;

  // Initial call returns false because of delay.
  EXPECT_FALSE(client().ShouldTriggerPopup(form_data, field, trigger_source));

  // Wait until segmentation blocks it (metrics logged).
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histograms.GetBucketCount(
               compose::kComposeProactiveNudgeShowStatus,
               compose::ComposeShowStatus::
                   kProactiveNudgeBlockedBySegmentationPlatform) == 1;
  }));

  // Commit metrics on page navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

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

  // Now call ShouldTriggerPopup again with delayed source and expect false.
  const autofill::AutofillSuggestionTriggerSource delayed_trigger_source =
      autofill::AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge;

  EXPECT_FALSE(
      client().ShouldTriggerPopup(form_data, field, delayed_trigger_source));

  // Check that even after a second call only one show status UMA was recorded.
  histograms.ExpectBucketCount(
      compose::kComposeProactiveNudgeShowStatus,
      compose::ComposeShowStatus::kProactiveNudgeBlockedBySegmentationPlatform,
      1);
}

// TODO(crbug.com/503556973): Re-enable after fixing flakiness on Windows,
// ChromeOS and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_TestShouldTriggerProactiveNudgeEnabled \
  DISABLED_TestShouldTriggerProactiveNudgeEnabled
#else
#define MAYBE_TestShouldTriggerProactiveNudgeEnabled \
  TestShouldTriggerProactiveNudgeEnabled
#endif
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       MAYBE_TestShouldTriggerProactiveNudgeEnabled) {
  base::HistogramTester histograms;

  // Enable proactive nudge.
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  config.proactive_nudge_focus_delay = base::Microseconds(4);
  config.proactive_nudge_segmentation = false;

  // Focus the field in the DOM to make it the active element.
  FocusField();

  autofill::FormFieldData field = field_data();
  field.set_origin(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  field.set_value(u"value0");
  field.set_form_control_type(autofill::FormControlType::kTextArea);
  field.set_allows_writing_suggestions(true);

  autofill::FormData form_data;
  form_data.set_url(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
  form_data.set_fields({field});

  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldValueChanged;

  // Should trigger after delay (using RunUntil to wait for timer/tasks).
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return client().ShouldTriggerPopup(form_data, field, trigger_source);
  }));

  // Commit metrics on page navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check that the proactive nudge UKM was captured.
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
  histograms.ExpectBucketCount(compose::kComposeProactiveNudgeCtr,
                               compose::ComposeNudgeCtrEvent::kNudgeDisplayed,
                               1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest, TestComposeNoParsedAny) {
  base::HistogramTester histograms;
  ShowDialogAndBindMojo();
  SetupMockModelExecution(1, std::nullopt);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> test_future;
  EXPECT_CALL(compose_dialog(), ResponseReceived(_))
      .WillOnce([&](compose::mojom::ComposeResponsePtr response) {
        test_future.SetValue(std::move(response));
      });

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);

  compose::mojom::ComposeResponsePtr result = test_future.Take();
  EXPECT_EQ(compose::mojom::ComposeStatus::kNoResponse, result->status);

  // Check that a request result No Response metric was emitted.
  histograms.ExpectUniqueSample(compose::kComposeRequestStatus,
                                compose::mojom::ComposeStatus::kNoResponse, 1);
  // Check that a request duration Error metric was emitted.
  histograms.ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationErrorSuffix}),
      1);
  // Check that no request duration OK metric was emitted.
  histograms.ExpectTotalCount(
      base::StrCat({"Compose", compose::kComposeRequestDurationOkSuffix}), 0);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeQualityOnlyOneLogEntryAbandonedOnClose) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution(2);

  // Wait for two log uploads.
  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(
      log_uploaded_signal.GetCallback().Then(base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(log_uploaded_signal.WaitAndClear());
        logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());
      })));

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);

  EXPECT_TRUE(compose_future.Wait());  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);

  EXPECT_TRUE(compose_future.Wait());
  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(2u, uploaded_logs().size());
  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_ABANDONED,
            uploaded_logs()[0]->compose().quality().final_status());
  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_UNSPECIFIED,
            uploaded_logs()[1]->compose().quality().final_status());
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeQualityNewSessionWithSelectedText) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution(2);

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Take());  // Reset future for second compose call.

  // Start a new session with selected text.
  autofill::FormFieldData field = field_data();
  field.set_value(u"user selected text");
  field.set_selected_text(u"selected text");
  ShowDialogAndBindMojoWithFieldData(field);

  // Get quality result from the abandoned session.
  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_ABANDONED,
            uploaded_logs()[0]->compose().quality().final_status());

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Take());

  // Close UI to submit remaining quality logs.
  log_uploaded_signal.Clear();
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(2u, uploaded_logs().size());
  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_ABANDONED,
            uploaded_logs()[1]->compose().quality().final_status());
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeQualityFinishedWithoutInsert) {
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution();

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Take());  // Reset future for second compose call.

  // Navigate to a new page.
  GURL next_page("http://example.com/a.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), next_page));

  // Get quality result from the abandoned session.
  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  EXPECT_EQ(
      optimization_guide::proto::FinalStatus::STATUS_FINISHED_WITHOUT_INSERT,
      uploaded_logs()[0]->compose().quality().final_status());
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeQualityFeedbackPositive) {
  base::HistogramTester histograms;
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution();

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  ShowDialogAndBindMojo();
  GetSessionForActiveComposeField()->SetSkipFeedbackUiForTesting(true);

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Take());

  page_handler()->SetUserFeedback(
      compose::mojom::UserFeedback::kUserFeedbackPositive);
  // Flush Mojo pipe to ensure feedback is recorded before closing UI.
  FlushPageHandler();

  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Get quality logs sent for the Compose Request.
  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  EXPECT_EQ(optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP,
            uploaded_logs()[0]->compose().quality().user_feedback());

  // Check that the histogram was sent for request feedback.
  histograms.ExpectUniqueSample(
      "Compose.Server.Request.Feedback",
      compose::ComposeRequestFeedback::kPositiveFeedback, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeQualityFeedbackNegative) {
  base::HistogramTester histograms;
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution();

  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());

  ShowDialogAndBindMojo();
  GetSessionForActiveComposeField()->SetSkipFeedbackUiForTesting(true);

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);
  EXPECT_TRUE(compose_future.Take());

  page_handler()->SetUserFeedback(
      compose::mojom::UserFeedback::kUserFeedbackNegative);
  // Flush Mojo pipe to ensure feedback is recorded before checking state or
  // closing UI.
  FlushPageHandler();

  // Also verify that the feedback state is correctly preserved in the session
  // state.
  base::test::TestFuture<compose::mojom::OpenMetadataPtr> initial_state_future;
  page_handler()->RequestInitialState(initial_state_future.GetCallback());
  auto open_metadata = initial_state_future.Take();
  EXPECT_EQ(compose::mojom::UserFeedback::kUserFeedbackNegative,
            open_metadata->compose_state->feedback);

  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  // Get quality logs sent for the Compose Request.
  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(1u, uploaded_logs().size());
  EXPECT_EQ(optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN,
            uploaded_logs()[0]->compose().quality().user_feedback());

  EXPECT_EQ(
      optimization_guide::proto::FinalModelStatus::FINAL_MODEL_STATUS_FAILURE,
      uploaded_logs()[0]->compose().quality().final_model_status());

  // Check that the histogram was sent for request feedback.
  histograms.ExpectUniqueSample(
      "Compose.Server.Request.Feedback",
      compose::ComposeRequestFeedback::kNegativeFeedback, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeQualityWasEdited) {
  base::HistogramTester histograms;
  ShowDialogAndBindMojo();

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  SetupMockModelExecution(2);

  // Wait for two log uploads.
  base::test::TestFuture<void> log_uploaded_signal;
  logs_uploader().WaitForLogUpload(
      log_uploaded_signal.GetCallback().Then(base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(log_uploaded_signal.WaitAndClear());
        logs_uploader().WaitForLogUpload(log_uploaded_signal.GetCallback());
      })));

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, false);

  EXPECT_TRUE(compose_future.Wait());  // Reset future for second compose call.
  compose_future.Clear();

  page_handler()->Compose("a user typed this",
                          compose::mojom::InputMode::kPolish, true);

  EXPECT_TRUE(compose_future.Wait());
  // Close UI to submit remaining quality logs.
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);

  EXPECT_TRUE(log_uploaded_signal.Wait());
  ASSERT_EQ(2u, uploaded_logs().size());
  EXPECT_TRUE(uploaded_logs()[0]->compose().quality().was_generated_via_edit());
  EXPECT_FALSE(
      uploaded_logs()[1]->compose().quality().was_generated_via_edit());
  EXPECT_EQ(optimization_guide::proto::FinalStatus::STATUS_UNSPECIFIED,
            uploaded_logs()[1]->compose().quality().final_status());

  histograms.ExpectBucketCount(
      compose::kComposeRequestReason,
      compose::ComposeRequestReason::kFirstRequestPolishMode, 1);
  histograms.ExpectBucketCount(
      "Compose.Server.Request.Reason",
      compose::ComposeRequestReason::kFirstRequestPolishMode, 1);
  histograms.ExpectBucketCount(compose::kComposeRequestReason,
                               compose::ComposeRequestReason::kUpdateRequest,
                               1);
  histograms.ExpectBucketCount("Compose.Server.Request.Reason",
                               compose::ComposeRequestReason::kUpdateRequest,
                               1);

  // Check that the histogram was sent for request feedback.
  histograms.ExpectUniqueSample("Compose.Server.Request.Feedback",
                                compose::ComposeRequestFeedback::kNoFeedback,
                                2);
}

// Tests that session level UKM metrics are properly captured after closing the
// dialog.
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest, TestCancelUkmMetrics) {
  ShowDialogAndBindMojo();
  client_page_handler()->CloseUI(compose::mojom::CloseReason::kCloseButton);
  // Make sure the async call to CloseUI completes before navigating away.
  client_page_handler().FlushForTesting();

  // Navigate page away to upload UKM metrics to the collector.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check session level UKM metrics.
  auto session_ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_SessionProgress::kEntryName,
      {ukm::builders::Compose_SessionProgress::kCanceledName});

  EXPECT_EQ(session_ukm_entries.size(), 1UL);

  EXPECT_THAT(session_ukm_entries[0].metrics,
              testing::UnorderedElementsAre(testing::Pair(
                  ukm::builders::Compose_SessionProgress::kCanceledName, 1)));
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_TestComposeShowContextMenu DISABLED_TestComposeShowContextMenu
#else
#define MAYBE_TestComposeShowContextMenu TestComposeShowContextMenu
#endif
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       MAYBE_TestComposeShowContextMenu) {
  auto* rfh = browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_content_editable_for_autofill = true;
  params.frame_origin = rfh->GetMainFrame()->GetLastCommittedOrigin();

  EXPECT_TRUE(client().ShouldTriggerContextMenu(rfh, params));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

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

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeShowContextMenuAndDialog) {
  auto* rfh = browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_content_editable_for_autofill = true;
  params.frame_origin = rfh->GetMainFrame()->GetLastCommittedOrigin();

  base::HistogramTester histograms;
  EXPECT_TRUE(client().ShouldTriggerContextMenu(rfh, params));
  ShowDialogAndBindMojo();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

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

  histograms.ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms.ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kComposeDialogOpened, 1);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestShouldTriggerProactiveNudgeDisabledUKM) {
  // Override country to "us" to ensure it's enabled for proactive nudge.
  auto country_override = ComposeEnabling::OverrideCountryForTesting("us");

  // Disable the proactive nudge
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = false;
  autofill::FormData form_data = CreateManualFormData();

  autofill::FormFieldData& selected_field_data =
      autofill::test_api(form_data).field(0);
  selected_field_data.set_origin(browser()
                                     ->tab_strip_model()
                                     ->GetActiveWebContents()
                                     ->GetPrimaryMainFrame()
                                     ->GetLastCommittedOrigin());
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldValueChanged;

  // By default the proactive nudge is disabled.
  EXPECT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));

  // Commit metrics on page navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

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

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestShouldTriggerProactiveNudgePageChecksFailUKM) {
  autofill::FormData form_data = CreateManualFormData(GURL("www.example.com"));

  autofill::FormFieldData& selected_field_data =
      autofill::test_api(form_data).field(0);
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldValueChanged;

  // Will fail because field origin does not match page origin.
  EXPECT_FALSE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                           trigger_source));

  // Commit metrics on page navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check that the proactive nudge UKM was not captured.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName});

  ASSERT_EQ(ukm_entries.size(), 0UL);
}

IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeShouldTriggerSavedStateNudgeUKM) {
  autofill::FormData form_data = CreateManualFormData();

  const autofill::FormFieldData& selected_field_data =
      autofill::test_api(form_data).field(0);
  const autofill::AutofillSuggestionTriggerSource trigger_source =
      autofill::AutofillSuggestionTriggerSource::kTextFieldValueChanged;

  // Start a Compose session on selected field.
  ShowDialogAndBindMojoWithFieldData(selected_field_data);

  // By default the saved state nudge is shown.
  EXPECT_TRUE(client().ShouldTriggerPopup(form_data, selected_field_data,
                                          trigger_source));

  // Commit metrics on page navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Check that no proactive nudge UKM was recorded.
  auto ukm_entries = ukm_recorder().GetEntries(
      ukm::builders::Compose_PageEvents::kEntryName,
      {ukm::builders::Compose_PageEvents::kMenuItemShownName,
       ukm::builders::Compose_PageEvents::kComposeTextInsertedName,
       ukm::builders::Compose_PageEvents::kProactiveNudgeShouldShowName});

  EXPECT_EQ(ukm_entries.size(), 0UL);
}

// Tests that Undo is not possible when Compose is never called and no response
// is ever received.
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest, TestEmptyUndo) {
  ShowDialogAndBindMojo();
  base::test::TestFuture<compose::mojom::ComposeStatePtr> test_future;
  page_handler()->Undo(test_future.GetCallback());
  EXPECT_FALSE(test_future.Take());
}

// Tests that Undo is not possible after only one Compose() invocation.
// TODO(b/334007229): incorporate redo testing.
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestUndoUnavailableFirstCompose) {
  ShowDialogAndBindMojo();
  SetupMockModelExecution(1);
  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);
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
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestComposeTwiceThenUpdateWebUIStateThenUndo) {
  base::HistogramTester histograms;
  ShowDialogAndBindMojo();
  SetupMockModelExecution(2);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  page_handler()->SaveWebUIState("this state should be restored with undo");
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";
  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);

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
  FlushPageHandler();

  // Check Compose Session Event Counts.
  histograms.ExpectBucketCount(
      compose::kComposeSessionEventCounts,
      compose::ComposeSessionEventTypes::kMainDialogShown, 1);
  histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                               compose::ComposeSessionEventTypes::kUndoClicked,
                               1);
  histograms.ExpectBucketCount(compose::kComposeSessionEventCounts,
                               compose::ComposeSessionEventTypes::kCloseClicked,
                               1);

  // Navigate page away to upload UKM metrics to the collector.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

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
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestUndoStackMultipleUndos) {
  ShowDialogAndBindMojo();
  SetupMockModelExecution(3);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  page_handler()->SaveWebUIState("first state");
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";
  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);
  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Second Compose() response should "
                                           "say undo is available.";

  page_handler()->SaveWebUIState("third state");
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);

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
IN_PROC_BROWSER_TEST_F(ChromeComposeClientBrowserTest,
                       TestUndoComposeThenUndoAgain) {
  ShowDialogAndBindMojo();
  SetupMockModelExecution(3);

  base::test::TestFuture<compose::mojom::ComposeResponsePtr> compose_future;
  BindComposeFutureToOnResponseReceived(compose_future);

  page_handler()->SaveWebUIState("first state");
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);

  compose::mojom::ComposeResponsePtr response = compose_future.Take();
  EXPECT_FALSE(response->undo_available) << "First Compose() response should "
                                            "say undo is not available.";

  page_handler()->SaveWebUIState("second state");
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);

  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Second Compose() response should "
                                           "say undo is available.";
  page_handler()->SaveWebUIState("wip web ui state");

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo_future;
  page_handler()->Undo(undo_future.GetCallback());
  EXPECT_EQ("first state", undo_future.Take()->webui_state);

  page_handler()->SaveWebUIState("third state");
  page_handler()->Compose("", compose::mojom::InputMode::kPolish, false);

  response = compose_future.Take();
  EXPECT_TRUE(response->undo_available) << "Third Compose() response should "
                                           "say undo is available.";

  base::test::TestFuture<compose::mojom::ComposeStatePtr> undo2_future;
  page_handler()->Undo(undo2_future.GetCallback());
  EXPECT_EQ("first state", undo2_future.Take()->webui_state);
}

class ChromeComposeClientLinksBrowserTest
    : public ChromeComposeClientBrowserTest {
 public:
  ChromeComposeClientLinksBrowserTest() {
    links_feature_list_.InitWithFeatures(
        {compose::features::kEnableCompose},
        {compose::features::kEnableComposeProactiveNudge});
  }

 private:
  base::test::ScopedFeatureList links_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeComposeClientLinksBrowserTest,
                       BugReportOpensCorrectURL) {
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

IN_PROC_BROWSER_TEST_F(ChromeComposeClientLinksBrowserTest,
                       LearnMoreLinkOpensCorrectURL) {
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

IN_PROC_BROWSER_TEST_F(ChromeComposeClientLinksBrowserTest,
                       SurveyLinkOpensCorrectURL) {
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
