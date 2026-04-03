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

  MockOptimizationGuideKeyedService& GetOptimizationGuide() {
    return *static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile()));
  }

  MockSegmentationPlatformService& GetSegmentationPlatformService() {
    return *static_cast<MockSegmentationPlatformService*>(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetForProfile(browser()->profile()));
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
