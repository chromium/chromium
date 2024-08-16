// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_page_handler.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/epoch_topics.h"
#include "components/browsing_topics/test_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace browsing_topics {

namespace {

const char kBrowsingTopicsInternalsUrl[] = "chrome://topics-internals/";
const char kBrowsingTopicsInternalsConsentInfoUrl[] =
    "chrome://topics-internals/#consent-info";

class FixedBrowsingTopicsService
    : public browsing_topics::BrowsingTopicsService {
 public:
  FixedBrowsingTopicsService() = default;
  ~FixedBrowsingTopicsService() override = default;

  bool HandleTopicsWebApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      ApiCallerSource caller_source,
      bool get_topics,
      bool observe,
      std::vector<blink::mojom::EpochTopicPtr>& topics) override {
    return false;
  }

  int NumVersionsInEpochs(const url::Origin& main_frame_origin) const override {
    return 0;
  }

  void GetBrowsingTopicsStateForWebUi(
      bool calculate_now,
      browsing_topics::mojom::PageHandler::GetBrowsingTopicsStateCallback
          callback) override {
    DCHECK(result_override_);

    std::move(callback).Run(result_override_->Clone());
  }

  std::vector<privacy_sandbox::CanonicalTopic> GetTopTopicsForDisplay()
      const override {
    return {};
  }

  void ValidateCalculationSchedule() override {}

  Annotator* GetAnnotator() override { return &test_annotator_; }

  void ClearTopic(
      const privacy_sandbox::CanonicalTopic& canonical_topic) override {}

  void ClearTopicsDataForOrigin(const url::Origin& origin) override {}

  void ClearAllTopicsData() override {}

  void SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResultPtr result) {
    result_override_ = std::move(result);
  }

  TestAnnotator* test_annotator() { return &test_annotator_; }

 private:
  browsing_topics::mojom::WebUIGetBrowsingTopicsStateResultPtr result_override_;
  TestAnnotator test_annotator_;
};

}  //  namespace

class BrowsingTopicsInternalsBrowserTestBase : public InProcessBrowserTest {
 public:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void FlushForTesting() {
    BrowsingTopicsInternalsUI* internals_ui =
        static_cast<BrowsingTopicsInternalsUI*>(
            web_contents()->GetWebUI()->GetController());

    BrowsingTopicsInternalsPageHandler* page_handler =
        internals_ui->page_handler();

    page_handler->FlushForTesting();
  }

  // Executing javascript in the WebUI requires using an isolated world in which
  // to execute the script because WebUI has a default CSP policy denying
  // "eval()", which is what EvalJs uses under the hood.
  std::string EvalJsInWebUI(const std::string& script) {
    return content::EvalJs(web_contents()->GetPrimaryMainFrame(), script,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           /*world_id=*/1)
        .ExtractString();
  }

  std::string GetModelInfoContent() {
    std::string html_content = EvalJsInWebUI(R"(
let result = '';

if (document.querySelector('#model-info-override-status-message-div').style.display !== 'none') {
  result += document.querySelector('#model-info-override-status-message-div').textContent + '\n';
}

if (document.querySelector('#model-info-div').style.display !== 'none') {
  result += document.querySelector('#model-version-div').textContent + '\n';
  result += document.querySelector('#model-file-path-div').textContent + '\n';
}

result
      )");

    return html_content;
  }

  std::string GetHostsClassificationInputValidationError() {
    std::string html_content = EvalJsInWebUI(R"(
let result = '';

let errorsDiv = document.querySelector('#hosts-classification-input-validation-error');

if (errorsDiv.style.display !== 'none') {
  Array.from(errorsDiv.children).forEach((errorDiv) => {
    result += errorDiv.textContent + '\n';
  });
}

result
      )");
    return html_content;
  }

  std::string GetHostsClassificationResultTableContent() {
    std::string html_content = EvalJsInWebUI(R"(
let result = '';

let tableWrapperDiv = document.querySelector('#hosts-classification-result-table-wrapper');

if (tableWrapperDiv.style.display === 'none') {
  result
} else {
  let rows = tableWrapperDiv.querySelectorAll('tr');

  rows.forEach(row => {
    let formattedRow = '';

    let cells = row.querySelectorAll('td');
    cells.forEach(cell => {
      let maybeSpans = cell.querySelectorAll('span');
      if (maybeSpans.length > 0) {
        let formattedCell = '';
        maybeSpans.forEach(span => {
          formattedCell += span.textContent + ';';
        });
        formattedRow += formattedCell + '|';
        return;
      }

      formattedRow += cell.textContent + '|';
    });

    if (formattedRow !== '') {
      result += formattedRow + '\n';
    }
  });
}

result
      )");

    return html_content;
  }

  std::string GetTopicsStateTabContent() {
    std::string html_content = EvalJsInWebUI(R"(
let result = '';

let overrideStatus = document.querySelector(
  '#topics-state-override-status-message-div').textContent;
if (overrideStatus) {
  result += 'overrideStatus: ' + overrideStatus + '\n';
}

if (document.querySelector('#topics-state-div').style.display === 'none') {
  result
} else {
  let nextCalculationTimeDiv = document.querySelector(
    '#next-scheduled-calculation-time-div');
  result += nextCalculationTimeDiv.textContent + '\n';

  let epochDivs = document.querySelectorAll('.epoch-div');
  epochDivs.forEach(epochDiv => {
    result += '===== epoch =====\n';
    let statusDivs = epochDiv.querySelectorAll('div');
    statusDivs.forEach(statusDiv => {
      result += statusDiv.textContent + '\n';
    });

    let rows = epochDiv.querySelectorAll('tr');

    rows.forEach(row => {
      let formattedRow = '';

      let cells = row.querySelectorAll('td');
      cells.forEach(cell => {
        let maybeSpans = cell.querySelectorAll('span');
        if (maybeSpans.length > 0) {
          let formattedCell = '';
          maybeSpans.forEach(span => {
            formattedCell += span.textContent + ';';
          });
          formattedRow += formattedCell + '|';
          return;
        }

        formattedRow += cell.textContent + '|';
      });

      if (formattedRow !== '') {
        result += formattedRow + '\n';
      }
    });
  });

  result
}
    )");

    // Skip checking the time field as it depends on the locale.
    RE2::GlobalReplace(&html_content, "time: .+",
                       "time: {{TIMESTAMP_TO_IGNORE}}");

    return html_content;
  }

  std::string GetFeaturesAndParametersTabContent() {
    std::string html_content = EvalJsInWebUI(R"(
let result = '';

let featureDivs = document.querySelector('.features-and-parameters-div')
  .querySelectorAll('div');
featureDivs.forEach(featureDiv => {
  result += featureDiv.textContent + '\n';
});

result
      )");

    return html_content;
  }

  std::string GetConsentInfoTabContent() {
    std::string html_content = EvalJsInWebUI(R"(
let result = '';

let consentDivs = document.querySelector('.consent-info-div')
  .querySelectorAll('div');
consentDivs.forEach(consentDiv => {
  result += consentDiv.textContent + '\n';
});

result
      )");

    return html_content;
  }

  std::string BuildExpectedConsentInfoString(int consent_status_string_id,
                                             int consent_source_string_id) {
    auto* privacy_sandbox_service =
        PrivacySandboxServiceFactory::GetForProfile(browser()->profile());

    auto consent_text = privacy_sandbox_service->TopicsConsentLastUpdateText();

    auto last_update_time = browser()->profile()->GetPrefs()->GetTime(
        prefs::kPrivacySandboxTopicsConsentLastUpdateTime);

    std::string expected_text =
        "{topicsConsentStatusLabel} {topicsConsentStatus}\n"
        "{topicsConsentSourceLabel} {topicsConsentSource}\n"
        "{topicsConsentTimeLabel} {topicsConsentTime}\n"
        "{topicsConsentTextLabel} {topicsConsentText}\n";

    RE2::Replace(&expected_text, "{topicsConsentStatusLabel}",
                 l10n_util::GetStringUTF8(
                     IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_STATUS_LABEL));
    RE2::Replace(
        &expected_text, "{topicsConsentSourceLabel}",
        l10n_util::GetStringUTF8(
            IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_LAST_UPDATE_SOURCE_LABEL));
    RE2::Replace(
        &expected_text, "{topicsConsentTimeLabel}",
        l10n_util::GetStringUTF8(
            IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_LAST_UPDATE_TIME_LABEL));
    RE2::Replace(
        &expected_text, "{topicsConsentTextLabel}",
        l10n_util::GetStringUTF8(
            IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_LAST_UPDATE_TEXT_LABEL));

    RE2::Replace(&expected_text, "{topicsConsentStatus}",
                 l10n_util::GetStringUTF8(consent_status_string_id));
    RE2::Replace(&expected_text, "{topicsConsentSource}",
                 l10n_util::GetStringUTF8(consent_source_string_id));
    RE2::Replace(&expected_text, "{topicsConsentTime}",
                 base::UTF16ToUTF8(
                     base::TimeFormatFriendlyDateAndTime(last_update_time)));
    RE2::Replace(&expected_text, "{topicsConsentText}", consent_text);

    return expected_text;
  }
};

class BrowsingTopicsDisabledInternalsBrowserTest
    : public BrowsingTopicsInternalsBrowserTestBase {
 public:
  BrowsingTopicsDisabledInternalsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            blink::features::kBrowsingTopics,
            blink::features::kBrowsingTopicsParameters,
            features::kPrivacySandboxAdsAPIsOverride,
        });
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledInternalsBrowserTest,
                       FeaturesDisabled) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  EXPECT_EQ(GetFeaturesAndParametersTabContent(), R"(BrowsingTopics: disabled
PrivacySandboxAdsAPIsOverride: disabled
OverridePrivacySandboxSettingsLocalTesting: disabled
BrowsingTopicsBypassIPIsPubliclyRoutableCheck: disabled
BrowsingTopicsDocumentAPI: enabled
Configuration version: 2
BrowsingTopicsParameters: disabled
BrowsingTopicsParameters:number_of_epochs_to_expose: 3
BrowsingTopicsParameters:time_period_per_epoch: 7d-0h-0m-0s
BrowsingTopicsParameters:number_of_top_topics_per_epoch: 5
BrowsingTopicsParameters:use_random_topic_probability_percent: 5
BrowsingTopicsParameters:max_epoch_introduction_delay: 2d-0h-0m-0s
BrowsingTopicsParameters:number_of_epochs_of_observation_data_to_use_for_filtering: 3
BrowsingTopicsParameters:max_number_of_api_usage_context_domains_to_keep_per_topic: 1000
BrowsingTopicsParameters:max_number_of_api_usage_context_entries_to_load_per_epoch: 100000
BrowsingTopicsParameters:max_number_of_api_usage_context_domains_to_store_per_page_load: 30
BrowsingTopicsParameters:taxonomy_version: 2
BrowsingTopicsParameters:disabled_topics_list: 
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledInternalsBrowserTest,
                       TopicsState_OverrideStatusMessage) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));
  EXPECT_EQ(
      GetTopicsStateTabContent(),
      R"(overrideStatus: No BrowsingTopicsService: the "BrowsingTopics" or other depend-on features are disabled.
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledInternalsBrowserTest,
                       ClassifierTab_OverrideStatusMessage) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  EXPECT_EQ(
      GetModelInfoContent(),
      R"(No BrowsingTopicsService: the "BrowsingTopics" or other depend-on features are disabled.
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledInternalsBrowserTest,
                       ConsentInfo_ConsentNotRequired) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(kBrowsingTopicsInternalsConsentInfoUrl)));

  auto consent_string = GetConsentInfoTabContent();
  auto expected_string = BuildExpectedConsentInfoString(
      IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_NOT_REQUIRED,
      IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_UPDATE_SOURCE_DEFAULT);

  EXPECT_EQ(expected_string, consent_string);
}

class BrowsingTopicsInternalsBrowserTest
    : public BrowsingTopicsInternalsBrowserTestBase {
 public:
  BrowsingTopicsInternalsBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kBrowsingTopicsParameters,
          {{"number_of_top_topics_per_epoch", "2"},
           {"time_period_per_epoch", "15s"}}},
         {blink::features::kBrowsingTopics, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {privacy_sandbox::kPrivacySandboxSettings4,
          {{"consent-required", "true"}}}},
        /*disabled_features=*/{});
  }

  // BrowserTestBase::SetUpInProcessBrowserTestFixture
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&BrowsingTopicsInternalsBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  FixedBrowsingTopicsService* fixed_browsing_topics_service() {
    return static_cast<FixedBrowsingTopicsService*>(
        browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(
            browser()->profile()));
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    browsing_topics::BrowsingTopicsServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating(&BrowsingTopicsInternalsBrowserTest::
                                             CreateFixedBrowsingTopicsService,
                                         base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateFixedBrowsingTopicsService(
      content::BrowserContext* context) {
    return std::make_unique<FixedBrowsingTopicsService>();
  }

  base::CallbackListSubscription subscription_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest, FeaturesEnabled) {
  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewOverrideStatusMessage("Failed to get the topics state."));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  EXPECT_EQ(GetFeaturesAndParametersTabContent(), R"(BrowsingTopics: enabled
PrivacySandboxAdsAPIsOverride: enabled
OverridePrivacySandboxSettingsLocalTesting: disabled
BrowsingTopicsBypassIPIsPubliclyRoutableCheck: disabled
BrowsingTopicsDocumentAPI: enabled
Configuration version: 2
BrowsingTopicsParameters: enabled
BrowsingTopicsParameters:number_of_epochs_to_expose: 3
BrowsingTopicsParameters:time_period_per_epoch: 0d-0h-0m-15s
BrowsingTopicsParameters:number_of_top_topics_per_epoch: 2
BrowsingTopicsParameters:use_random_topic_probability_percent: 5
BrowsingTopicsParameters:max_epoch_introduction_delay: 2d-0h-0m-0s
BrowsingTopicsParameters:number_of_epochs_of_observation_data_to_use_for_filtering: 3
BrowsingTopicsParameters:max_number_of_api_usage_context_domains_to_keep_per_topic: 1000
BrowsingTopicsParameters:max_number_of_api_usage_context_entries_to_load_per_epoch: 100000
BrowsingTopicsParameters:max_number_of_api_usage_context_domains_to_store_per_page_load: 30
BrowsingTopicsParameters:taxonomy_version: 2
BrowsingTopicsParameters:disabled_topics_list: 
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest,
                       TopicsState_OverrideStatusMessage) {
  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewOverrideStatusMessage("Failed to get the topics state."));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  EXPECT_EQ(GetTopicsStateTabContent(),
            R"(overrideStatus: Failed to get the topics state.
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest,
                       TopicsState_Populated) {
  auto state = browsing_topics::mojom::WebUIBrowsingTopicsState::New();

  state->next_scheduled_calculation_time = base::Time::Now();

  {
    auto webui_epoch = mojom::WebUIEpoch::New();
    webui_epoch->calculation_time = base::Time::Now() - base::Days(7);
    webui_epoch->model_version = "2204011803";
    webui_epoch->taxonomy_version = "123";
    {
      auto webui_topic = mojom::WebUITopic::New();
      webui_topic->topic_id = 2;
      webui_topic->topic_name = u"Acting & Theater";
      webui_topic->is_real_topic = true;
      webui_topic->observed_by_domains = {"111", "222", "333"};
      webui_epoch->topics.push_back(std::move(webui_topic));
    }

    {
      auto webui_topic = mojom::WebUITopic::New();
      webui_topic->topic_id = 3;
      webui_topic->topic_name = u"Comics";
      webui_topic->is_real_topic = true;
      webui_topic->observed_by_domains = {"444"};
      webui_epoch->topics.push_back(std::move(webui_topic));
    }

    state->epochs.push_back(std::move(webui_epoch));
  }

  {
    auto webui_epoch = mojom::WebUIEpoch::New();
    webui_epoch->calculation_time = base::Time::Now() - base::Days(7);
    webui_epoch->model_version = "2204011803";
    webui_epoch->taxonomy_version = "123";
    {
      auto webui_topic = mojom::WebUITopic::New();
      webui_topic->topic_id = 4;
      webui_topic->topic_name = u"Concerts & Music Festivals";
      webui_topic->is_real_topic = true;
      webui_topic->observed_by_domains = {};
      webui_epoch->topics.push_back(std::move(webui_topic));
    }

    {
      auto webui_topic = mojom::WebUITopic::New();
      webui_topic->topic_id = 5;
      webui_topic->topic_name = u"Concerts & Music Festivals";
      webui_topic->is_real_topic = false;
      webui_topic->observed_by_domains = {"555"};
      webui_epoch->topics.push_back(std::move(webui_topic));
    }

    state->epochs.push_back(std::move(webui_epoch));
  }

  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewBrowsingTopicsState(std::move(state)));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  EXPECT_EQ(GetTopicsStateTabContent(),
            R"(Next scheduled calculation time: {{TIMESTAMP_TO_IGNORE}}
===== epoch =====
Calculation time: {{TIMESTAMP_TO_IGNORE}}
Model version: 2204011803
Taxonomy version: 123
2|Acting & Theater|Real|111;222;333;|
3|Comics|Real|444;|
===== epoch =====
Calculation time: {{TIMESTAMP_TO_IGNORE}}
Model version: 2204011803
Taxonomy version: 123
4|Concerts & Music Festivals|Real||
5|Concerts & Music Festivals|Random|555;|
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest,
                       TopicsState_CalculateNow) {
  auto state = browsing_topics::mojom::WebUIBrowsingTopicsState::New();

  state->next_scheduled_calculation_time = base::Time::Now();

  {
    auto webui_epoch = mojom::WebUIEpoch::New();
    webui_epoch->calculation_time = base::Time::Now() - base::Days(7);
    webui_epoch->model_version = "2204011803";
    webui_epoch->taxonomy_version = "123";
    {
      auto webui_topic = mojom::WebUITopic::New();
      webui_topic->topic_id = 2;
      webui_topic->topic_name = u"Acting & Theater";
      webui_topic->is_real_topic = true;
      webui_topic->observed_by_domains = {"111", "222", "333"};
      webui_epoch->topics.push_back(std::move(webui_topic));
    }
    state->epochs.push_back(std::move(webui_epoch));
  }

  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewBrowsingTopicsState(std::move(state)));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  EXPECT_EQ(GetTopicsStateTabContent(),
            R"(Next scheduled calculation time: {{TIMESTAMP_TO_IGNORE}}
===== epoch =====
Calculation time: {{TIMESTAMP_TO_IGNORE}}
Model version: 2204011803
Taxonomy version: 123
2|Acting & Theater|Real|111;222;333;|
)");

  auto new_state = browsing_topics::mojom::WebUIBrowsingTopicsState::New();

  new_state->next_scheduled_calculation_time = base::Time::Now();

  {
    auto webui_epoch = mojom::WebUIEpoch::New();
    webui_epoch->calculation_time = base::Time::Now() - base::Days(7);
    webui_epoch->model_version = "2204011803";
    webui_epoch->taxonomy_version = "123";
    {
      auto webui_topic = mojom::WebUITopic::New();
      webui_topic->topic_id = 4;
      webui_topic->topic_name = u"Concerts & Music Festivals";
      webui_topic->is_real_topic = true;
      webui_topic->observed_by_domains = {};
      webui_epoch->topics.push_back(std::move(webui_topic));
    }

    new_state->epochs.push_back(std::move(webui_epoch));
  }

  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewBrowsingTopicsState(std::move(new_state)));

  constexpr char calculate_now_script[] = R"(
    document.querySelector('#calculate-now-button').click();

    // Assert that buttons are disabled during a Calculate Now request.
    if (!document.querySelector('#refresh-topics-state-button').disabled ||
        !document.querySelector('#calculate-now-button').disabled) {
      throw "Buttons should be disabled";
    }
  )";

  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     calculate_now_script,
                     content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                     /*world_id=*/1));

  EXPECT_EQ(GetTopicsStateTabContent(),
            R"(Next scheduled calculation time: {{TIMESTAMP_TO_IGNORE}}
===== epoch =====
Calculation time: {{TIMESTAMP_TO_IGNORE}}
Model version: 2204011803
Taxonomy version: 123
4|Concerts & Music Festivals|Real||
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest,
                       ClassifierTab_ModelUnavailable) {
  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewOverrideStatusMessage("Failed to get the topics state."));

  // The |ModelInfo| is not set so the model will not be marked as available.

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  EXPECT_EQ(GetModelInfoContent(),
            R"(Model unavailable.
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest, ClassifierTab) {
  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewOverrideStatusMessage("Failed to get the topics state."));

  // Configure the (mock) model.

  fixed_browsing_topics_service()->test_annotator()->UseModelInfo(
      *optimization_guide::TestModelInfoBuilder()
           .SetVersion(1)
           .SetModelFilePath(
               base::FilePath::FromASCII("/test_path/test_model.tflite"))
           .Build());
  fixed_browsing_topics_service()->test_annotator()->UseAnnotations({
      {"foo1.com", {1, 2}},
      {"foo2.com", {3, 4, 5}},
  });

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  EXPECT_EQ(GetModelInfoContent(),
            R"(Model version: 1
Model file path: /test_path/test_model.tflite
)");

  constexpr char classify_hosts_script[] = R"(
    document.querySelector('#input-hosts-textarea').value = 'foo1.com\nfoo2.com\n';
    document.querySelector('#hosts-classification-button').click();
  )";

  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     classify_hosts_script,
                     content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                     /*world_id=*/1));

  FlushForTesting();

  EXPECT_EQ(GetHostsClassificationResultTableContent(),
            R"(foo1.com|1. Arts & Entertainment;2. Acting & Theater;|
foo2.com|3. Comics;4. Concerts & Music Festivals;5. Dance;|
)");

  EXPECT_TRUE(GetHostsClassificationInputValidationError().empty());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest,
                       ClassifierTab_InvalidInput) {
  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewOverrideStatusMessage("Failed to get the topics state."));

  // Configure the (mock) model.

  fixed_browsing_topics_service()->test_annotator()->UseModelInfo(
      *optimization_guide::TestModelInfoBuilder()
           .SetVersion(1)
           .SetModelFilePath(
               base::FilePath::FromASCII("/test_path/test_model.tflite"))
           .Build());
  fixed_browsing_topics_service()->test_annotator()->UseAnnotations({
      {"foo1.com", {1, 2}},
      {"foo2.com", {3, 4, 5}},
  });

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  constexpr char classify_hosts_script[] = R"(
    document.querySelector('#input-hosts-textarea').value = 'foo1.com\nhttps://foo1.com\nfoo1.com/path';
    document.querySelector('#hosts-classification-button').click();
  )";

  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     classify_hosts_script,
                     content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                     /*world_id=*/1));

  EXPECT_TRUE(GetHostsClassificationResultTableContent().empty());

  EXPECT_EQ(GetHostsClassificationInputValidationError(),
            R"(Host "https://foo1.com" contains invalid character: "/"
Host "foo1.com/path" contains invalid character: "/"
)");
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest,
                       ConsentInfo_RequiresFragment) {
  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewOverrideStatusMessage("Failed to get the topics state."));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(kBrowsingTopicsInternalsUrl)));

  constexpr char consent_tab_display[] = R"(
    let element = document.querySelector('#consent-info')
    window.getComputedStyle(element).display
  )";

  EXPECT_EQ("none", EvalJsInWebUI(consent_tab_display))
      << "Consent info tab should be hidden if not fragment target";

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(kBrowsingTopicsInternalsConsentInfoUrl)));

  EXPECT_EQ("block", EvalJsInWebUI(consent_tab_display))
      << "Consent info tab should be visible if fragment target";
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest,
                       ConsentInfo_ActiveConsent) {
  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewOverrideStatusMessage("Failed to get the topics state."));

  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());

  privacy_sandbox_service->PromptActionOccurred(
      PrivacySandboxService::PromptAction::kConsentAccepted,
      PrivacySandboxService::SurfaceType::kDesktop);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(kBrowsingTopicsInternalsConsentInfoUrl)));

  auto consent_string = GetConsentInfoTabContent();
  auto expected_string = BuildExpectedConsentInfoString(
      IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_ACTIVE,
      IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_UPDATE_SOURCE_CONFIRMATION);

  EXPECT_EQ(expected_string, consent_string);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsInternalsBrowserTest,
                       ConsentInfo_InactiveConsent) {
  fixed_browsing_topics_service()->SetWebUIGetBrowsingTopicsStateResultOverride(
      browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
          NewOverrideStatusMessage("Failed to get the topics state."));

  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(browser()->profile());

  privacy_sandbox_service->TopicsToggleChanged(/*new_value=*/false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(kBrowsingTopicsInternalsConsentInfoUrl)));

  auto consent_string = GetConsentInfoTabContent();
  auto expected_string = BuildExpectedConsentInfoString(
      IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_INACTIVE,
      IDS_PRIVACY_SANDBOX_TOPICS_CONSENT_UPDATE_SOURCE_SETTINGS);

  EXPECT_EQ(expected_string, consent_string);
}

}  // namespace browsing_topics
