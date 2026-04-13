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

  void SetPrefsForComposeMSBBState(bool msbb_state) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        msbb_state);
  }

  // Sets up mock model execution. `times` specifies the expected number of
  // calls. Default is 1, which is functionally equivalent to `WillOnce()`
  // because the action is identical for all calls.
  void SetupMockModelExecution(
      int times = 1,
      const std::string& response_output = "Cucumbers") {
    auto action = testing::WithArg<3>(
        [response_output](
            optimization_guide::OptimizationGuideModelExecutionResultCallback
                callback) {
          optimization_guide::proto::ComposeResponse response;
          response.set_output(response_output);
          std::move(callback).Run(
              OptimizationGuideModelExecutionResult(
                  base::ok(optimization_guide::AnyWrapProto(response)),
                  std::make_unique<
                      optimization_guide::proto::ModelExecutionInfo>()),
              /*model_quality_log_entry=*/nullptr);
        });

    EXPECT_CALL(model_executor(),
                ExecuteModel(testing::_, testing::_, testing::_, testing::_))
        .Times(times)
        .WillRepeatedly(std::move(action));
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

  compose::mojom::ComposeSessionUntrustedPageHandler* page_handler() {
    return page_handler_.get();
  }

  MockComposeDialog& compose_dialog() { return compose_dialog_; }

  testing::NiceMock<optimization_guide::MockRemoteModelExecutor>&
  model_executor() {
    return model_executor_;
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
