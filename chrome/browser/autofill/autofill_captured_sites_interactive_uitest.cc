// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_flow_test_util.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/automated_tests/cache_replayer.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/data_element.h"
#include "testing/gtest/include/gtest/gtest.h"

using captured_sites_test_utils::CapturedSiteParams;
using captured_sites_test_utils::GetCapturedSites;
using captured_sites_test_utils::TestRecipeReplayer;
using captured_sites_test_utils::WebPageReplayServerWrapper;

namespace autofill {
namespace {

// The timeout for actions like bringing up the Autofill popup or showing the
// preview of suggestions.
constexpr base::TimeDelta kAutofillWaitForActionInterval = base::Seconds(5);
// The timeout for autofilling a form. This is much higher than for other
// actions because autofilling may trigger expensive JavaScript activity.
// It may also be expensive due to CVC validation.
constexpr base::TimeDelta kAutofillWaitForFillInterval = base::Seconds(60);

base::FilePath GetReplayFilesRootDirectory() {
  base::FilePath src_dir;
  if (base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir)) {
    return src_dir.AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("autofill")
        .AppendASCII("captured_sites")
        .AppendASCII("artifacts");
  } else {
    src_dir.clear();
    return src_dir;
  }
}

autofill::ElementExpr GetElementByXpath(const std::string& xpath) {
  return autofill::ElementExpr(base::StringPrintf(
      "automation_helper.getElementByXpath(`%s`)", xpath.c_str()));
}

// Implements the `kAutofillCapturedSiteTestsMetricsScraper` testing feature.
class MetricsScraper {
 public:
  // Creates a MetricsScraper if the Finch flag is enabled.
  static std::unique_ptr<MetricsScraper> MaybeCreate(const std::string& test) {
    if (!base::FeatureList::IsEnabled(
            features::test::kAutofillCapturedSiteTestsMetricsScraper)) {
      return nullptr;
    }
    const std::string& output_dir =
        features::test::kAutofillCapturedSiteTestsMetricsScraperOutputDir.Get();
    const std::string& histogram_regex =
        features::test::kAutofillCapturedSiteTestsMetricsScraperHistogramRegex
            .Get();
    return base::WrapUnique(new MetricsScraper(
        base::FilePath::FromASCII(output_dir).AppendASCII(test + ".txt"),
        base::UTF8ToUTF16(histogram_regex)));
  }

  // Writes all samples of all histograms matching `histogram_regex_` into
  // `output_file_`.
  void ScrapeMetrics() const {
    // Combine metrics from different processes.
    ::metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    // Get all relevant histogram names. Since `GetHistograms()` doesn't
    // guarantee order, the results are sorted for easier comparisons across
    // runs.
    std::vector<std::string> histogram_names;
    for (base::HistogramBase* histogram :
         base::StatisticsRecorder::GetHistograms()) {
      const std::string& name = histogram->histogram_name();
      if (MatchesRegex(base::UTF8ToUTF16(name), *histogram_regex_)) {
        histogram_names.push_back(name);
      }
    }
    base::ranges::sort(histogram_names);

    // Output the samples of all `histogram_names` to `output_file`.
    std::stringstream output;
    for (const std::string& name : histogram_names) {
      output << name << std::endl;
      for (const base::Bucket& bucket : histogram_tester_.GetAllSamples(name)) {
        output << bucket.min << " " << bucket.count << std::endl;
      }
    }
    base::WriteFile(output_file_, output.str());
  }

 private:
  MetricsScraper(const base::FilePath& output_file,
                 std::u16string_view histogram_regex)
      : output_file_(output_file),
        histogram_regex_(CompileRegex(histogram_regex)) {}

  const base::FilePath output_file_;
  const std::unique_ptr<const icu::RegexPattern> histogram_regex_;
  const base::HistogramTester histogram_tester_;
};


class AutofillCapturedSitesInteractiveTest
    : public AutofillUiTest,
      public captured_sites_test_utils::
          TestRecipeReplayChromeFeatureActionExecutor,
      public ::testing::WithParamInterface<CapturedSiteParams> {
 public:
  // TestRecipeReplayChromeFeatureActionExecutor
  bool AutofillForm(const std::string& focus_element_css_selector,
                    const std::vector<std::string>& iframe_path,
                    const int attempts,
                    content::RenderFrameHost* frame,
                    std::optional<FieldType> triggered_field_type) override {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(frame);
    auto& autofill_manager = static_cast<BrowserAutofillManager&>(
        ContentAutofillDriver::GetForRenderFrameHost(frame->GetMainFrame())
            ->GetAutofillManager());
    test_delegate()->Observe(autofill_manager);

    if (base::FeatureList::IsEnabled(
            features::test::kAutofillCapturedSiteTestsUseAutofillFlow)) {
      if (AutofillFormWithAutofillFlow(web_contents, focus_element_css_selector,
                                       attempts, frame, triggered_field_type)) {
        return true;
      }
      VLOG(1) << "Attempted to use AutofillFlow, but failed. Trying backup...";
    }

    int tries = 0;
    while (tries < attempts) {
      tries++;
      LOG(INFO) << "Autofill attempt " << tries << " of " << attempts;

      // Translation bubbles and address-save prompts and others may overlap
      // with and thus prevent the Autofill popup, so we preemptively close all
      // bubbles.
      translate::test_utils::CloseCurrentBubble(browser());
      TryToCloseAllPrompts(web_contents);

      autofill_manager.client().HideAutofillSuggestions(
          autofill::SuggestionHidingReason::kViewDestroyed);

      testing::AssertionResult suggestions_shown = ShowAutofillSuggestion(
          focus_element_css_selector, iframe_path, frame);
      if (!suggestions_shown) {
        LOG(WARNING) << "Failed to bring up the autofill suggestion drop down: "
                     << suggestions_shown.message();
        continue;
      }

      // Press the down key to highlight the first choice in the autofill
      // suggestion drop down.
      test_delegate()->SetExpectations({ObservedUiEvents::kPreviewFormData},
                                       kAutofillWaitForActionInterval);
      SendKeyToPopup(frame, ui::DomKey::ARROW_DOWN);
      testing::AssertionResult preview_shown = test_delegate()->Wait();
      if (!preview_shown) {
        LOG(WARNING) << "Failed to select an option from the "
                     << "autofill suggestion drop down: "
                     << preview_shown.message();
        continue;
      }

      std::optional<std::u16string> cvc = profile_controller_->cvc();
      // If CVC is available in the Action Recorder receipts and this is a
      // payment form, this means it's running the test with a server card. So
      // the "Enter CVC" dialog will pop up for card autofill.
      // TODO(crbug.com/333815150): Fix the TestCardUnmaskPromptWaiter.
      bool is_credit_card_field =
          triggered_field_type.has_value() &&
          GroupTypeOfFieldType(triggered_field_type.value()) ==
              FieldTypeGroup::kCreditCard;
      bool should_cvc_dialog_pop_up = is_credit_card_field && cvc;
      CHECK(!should_cvc_dialog_pop_up)
          << "Tests with CVC dialogs are currently not supported due to "
             "crbug.com/333815150. See crrev.com/c/5458703 for the code to "
             "bring back the TestCardUnmaskPromptWaiter.";

      // Press the enter key to invoke autofill using the first suggestion.
      test_delegate()->SetExpectations({ObservedUiEvents::kFormDataFilled,
                                        ObservedUiEvents::kSuggestionsHidden},
                                       kAutofillWaitForFillInterval);
      SendKeyToPopup(frame, ui::DomKey::ENTER);
      testing::AssertionResult form_filled = test_delegate()->Wait();
      if (!form_filled) {
        LOG(WARNING) << "Failed to fill the form: " << form_filled.message();
        continue;
      }

      return true;
    }

    autofill_manager.client().HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kViewDestroyed);
    ADD_FAILURE() << "Failed to autofill the form!";
    return false;
  }

  bool AddAutofillProfileInfo(const std::string& field_type,
                              const std::string& field_value) override {
    return profile_controller_->AddAutofillProfileInfo(field_type, field_value);
  }

  bool SetupAutofillProfile() override {
    AddTestAutofillData(browser()->profile(), profile_controller_->profile(),
                        profile_controller_->credit_card());
    // Disable the Password Manager to prevent password bubbles from occurring.
    // The password bubbles could overlap with the Autofill popups, in which
    // case the Autofill popup would not be shown (crbug.com/1223898).
    browser()->profile()->GetPrefs()->SetBoolean(
        password_manager::prefs::kCredentialsEnableService, false);
    return true;
  }

 protected:
  AutofillCapturedSitesInteractiveTest()
      : AutofillUiTest({.disable_server_communication = false}) {}

  ~AutofillCapturedSitesInteractiveTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AutofillUiTest::SetUpOnMainThread();
    test_delegate()->SetIgnoreBackToBackMessages(
        ObservedUiEvents::kPreviewFormData, true);
    test_delegate()->SetIgnoreBackToBackMessages(
        ObservedUiEvents::kFormDataFilled, true);
    recipe_replayer_ =
        std::make_unique<captured_sites_test_utils::TestRecipeReplayer>(
            browser(), this);
    profile_controller_ =
        std::make_unique<captured_sites_test_utils::ProfileDataController>();

    SetServerUrlLoader(std::make_unique<test::ServerUrlLoader>(
        std::make_unique<test::ServerCacheReplayer>(
            GetParam().capture_file_path,
            test::ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                test::ServerCacheReplayer::kOptionSplitRequestsByForm)));

    metrics_scraper_ = MetricsScraper::MaybeCreate(GetParam().site_name);

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 false);
  }

  void TearDownOnMainThread() override {
    if (metrics_scraper_) {
      metrics_scraper_->ScrapeMetrics();
    }
    recipe_replayer_.reset();
    // Need to delete the URL loader and its underlying interceptor on the main
    // thread. Will result in a fatal crash otherwise. The pointer has its
    // memory cleaned up twice: first time in that single thread, a second time
    // when the fixture's destructor is called, which will have no effect since
    // the raw pointer will be nullptr.
    server_url_loader_.reset();
    AutofillUiTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Allow access exception to live Autofill Server for
    // overriding cache replay behavior.
    host_resolver()->AllowDirectLookup("clients1.google.com");
    host_resolver()->AllowDirectLookup("content-autofill.googleapis.com");
    AutofillUiTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void SetUpHostResolverRules(base::CommandLine* command_line) {
    captured_sites_test_utils::TestRecipeReplayer::SetUpHostResolverRules(
        command_line);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the autofill show typed prediction feature. When active this
    // feature forces input elements on a form to display their autofill type
    // prediction. Test will check this attribute on all the relevant input
    // elements in a form to determine if the form is ready for interaction.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::test::kAutofillServerCommunication,
                               {}},
                              {features::test::kAutofillShowTypePredictions,
                               {}},
                              {features::test::
                                   kAutofillCapturedSiteTestsUseAutofillFlow,
                               {}}},
        /*disabled_features=*/{features::kAutofillOverwritePlaceholdersOnly,
                               features::kAutofillSkipPreFilledFields});
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, "us");
    // SelectParserRelaxation affects the results from the test data because the
    // test data may have unclosed <select> tags. Since SelectParserRelaxation
    // is not enabled by default, we are disabling it for these tests.
    command_line->AppendSwitchASCII("disable-blink-features",
                                    "SelectParserRelaxation");
    AutofillUiTest::SetUpCommandLine(command_line);
    SetUpHostResolverRules(command_line);
    captured_sites_test_utils::TestRecipeReplayer::SetUpCommandLine(
        command_line);
  }

  void SetServerUrlLoader(
      std::unique_ptr<test::ServerUrlLoader> server_url_loader) {
    server_url_loader_ = std::move(server_url_loader);
  }

  captured_sites_test_utils::TestRecipeReplayer* recipe_replayer() {
    return recipe_replayer_.get();
  }

 private:
  [[nodiscard]] testing::AssertionResult ShowAutofillSuggestion(
      const std::string& target_element_xpath,
      const std::vector<std::string>& iframe_path,
      content::RenderFrameHost* frame) {
    auto disable_popup_timing_checks = [&frame]() {
      auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
      CHECK_NE(web_contents, nullptr);
      auto* client =
          ChromeAutofillClient::FromWebContentsForTesting(web_contents);
      CHECK_NE(client, nullptr);
      if (base::WeakPtr<AutofillSuggestionController> controller =
              client->suggestion_controller_for_testing()) {
        test_api(static_cast<AutofillPopupControllerImpl&>(*controller))
            .DisableThreshold(true);
      }
    };
    // First, automation should focus on the frame containing the autofill
    // form. Doing so ensures that Chrome scrolls the element into view if
    // the element is off the page.
    test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionsShown},
                                     kAutofillWaitForActionInterval);
    if (!captured_sites_test_utils::TestRecipeReplayer::PlaceFocusOnElement(
            target_element_xpath, iframe_path, frame)) {
      return testing::AssertionFailure()
             << "PlaceFocusOnElement() failed in " << FROM_HERE.ToString();
    }
    if (test_delegate()->Wait()) {
      disable_popup_timing_checks();
      return testing::AssertionSuccess();
    }

    gfx::Rect rect;
    if (!captured_sites_test_utils::TestRecipeReplayer::
            GetBoundingRectOfTargetElement(target_element_xpath, iframe_path,
                                           frame, &rect)) {
      return testing::AssertionFailure()
             << "GetBoundingRectOfTargetElement() failed in "
             << FROM_HERE.ToString();
    }

    test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionsShown},
                                     kAutofillWaitForActionInterval);
    if (!captured_sites_test_utils::TestRecipeReplayer::
            SimulateLeftMouseClickAt(rect.CenterPoint(), frame))
      return testing::AssertionFailure()
             << "SimulateLeftMouseClickAt() failed in " << FROM_HERE.ToString();

    auto result = test_delegate()->Wait();
    disable_popup_timing_checks();
    return result;
  }

  bool AutofillFormWithAutofillFlow(
      content::WebContents* web_contents,
      const std::string& focus_element_css_selector,
      const int attempts,
      content::RenderFrameHost* frame,
      std::optional<FieldType> triggered_field_type) {
    std::optional<std::u16string> cvc = profile_controller_->cvc();
    // If CVC is available in the Action Recorder receipts and this is a
    // payment form, this means it's running the test with a server card. So
    // the "Enter CVC" dialog will pop up for card autofill.
    // TODO(crbug.com/333815150): Fix the TestCardUnmaskPromptWaiter.
    bool is_credit_card_field =
        triggered_field_type.has_value() &&
        GroupTypeOfFieldType(triggered_field_type.value()) ==
            FieldTypeGroup::kCreditCard;
    bool should_cvc_dialog_pop_up = is_credit_card_field && cvc;
    CHECK(!should_cvc_dialog_pop_up)
        << "Tests with CVC dialogs are currently not supported due to "
           "crbug.com/333815150. See crrev.com/c/5458703 for the code to bring "
           "back the TestCardUnmaskPromptWaiter.";

    // Use AutofillFlow library to trigger the autofill behavior. Try both ways.
    testing::AssertionResult autofill_assertion_by_arrow =
        AutofillFlow(GetElementByXpath(focus_element_css_selector), this,
                     {.show_method = ShowMethod::ByArrow(),
                      .max_show_tries = static_cast<size_t>(attempts),
                      .execution_target = frame});
    if (autofill_assertion_by_arrow) {
      VLOG(1) << "Successful trigger autofill via 'ByArrow':";
    } else {
      VLOG(1) << "Failed to trigger autofill via 'ByArrow':"
              << autofill_assertion_by_arrow.message()
              << "\nFalling back to via 'ByClick'";

      testing::AssertionResult autofill_assertion_by_click =
          AutofillFlow(GetElementByXpath(focus_element_css_selector), this,
                       {.show_method = ShowMethod::ByClick(),
                        .max_show_tries = static_cast<size_t>(attempts),
                        .execution_target = frame});
      if (autofill_assertion_by_click) {
        VLOG(1) << "Successful trigger autofill via 'ByClick':";
      } else {
        VLOG(1) << "Failed to trigger autofill via 'ByClick':"
                << autofill_assertion_by_click.message()
                << "\nNo Fallbacks left'";
        return false;
      }
    }
    return true;
  }

  std::unique_ptr<captured_sites_test_utils::TestRecipeReplayer>
      recipe_replayer_;
  std::unique_ptr<captured_sites_test_utils::ProfileDataController>
      profile_controller_;

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<test::ServerUrlLoader> server_url_loader_;
  std::unique_ptr<MetricsScraper> metrics_scraper_;
};

IN_PROC_BROWSER_TEST_P(AutofillCapturedSitesInteractiveTest, Recipe) {
  captured_sites_test_utils::PrintInstructions(
      "autofill_captured_sites_interactive_uitest");

  // Prints the name of the site to be executed. Prints bug number if exists.
  if (GetParam().bug_number) {
    VLOG(1) << GetParam().site_name << ": crbug.com/"
            << GetParam().bug_number.value();
  } else {
    VLOG(1) << GetParam().site_name;
  }

  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));

  bool test_completed = recipe_replayer()->ReplayTest(
      GetParam().capture_file_path, GetParam().recipe_file_path,
      captured_sites_test_utils::GetCommandFilePath());
  if (!test_completed)
    ADD_FAILURE() << "Full execution was unable to complete.";

  std::vector<testing::AssertionResult> validation_failures =
      recipe_replayer()->GetValidationFailures();
  if (GetParam().expectation == captured_sites_test_utils::kPass) {
    if (validation_failures.empty()) {
      VLOG(1) << "No Validation Failures";
    } else {
      LOG(INFO) << "There were " << validation_failures.size()
              << " Validation Failure(s)";
      for (auto& validation_failure : validation_failures)
        ADD_FAILURE() << validation_failure.message();
    }
  } else {
    if (validation_failures.empty()) {
      VLOG(1) << "Expected Validation Failures but still succeeded. "
              << "Time to update testcases.json file?";
    } else {
      VLOG(1) << "Validation Failures expected and received so skipping";
      for (auto& validation_failure : validation_failures)
        VLOG(1) << validation_failure.message();
      GTEST_SKIP();
    }
  }
}

// This test is called with a dynamic list and will be empty during the Password
// run instance, so adding GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST a la
// crbug/1192206
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    AutofillCapturedSitesInteractiveTest);
INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillCapturedSitesInteractiveTest,
    testing::ValuesIn(GetCapturedSites(GetReplayFilesRootDirectory())),
    captured_sites_test_utils::GetParamAsString());

class AutofillCapturedSitesRefresh
    : public AutofillCapturedSitesInteractiveTest {
 protected:
  void SetUpOnMainThread() override {
    AutofillCapturedSitesInteractiveTest::SetUpOnMainThread();
    web_page_replay_server_wrapper_ =
        std::make_unique<captured_sites_test_utils::WebPageReplayServerWrapper>(
            false, 8082, 8083);
  }

  void SetUpHostResolverRules(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        base::StringPrintf(
            "MAP *:80 127.0.0.1:%d,"
            "MAP *.googleapis.com:443 127.0.0.1:%d,"
            "MAP *:443 127.0.0.1:%d,"
            // Set to always exclude, allows cache_replayer overwrite
            "EXCLUDE localhost",
            TestRecipeReplayer::kHostHttpPort,
            TestRecipeReplayer::kHostHttpsRecordPort,
            TestRecipeReplayer::kHostHttpsPort));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // For refresh tests, ensure that responses for Server Predictions come from
    // the Production Server and not the CacheReplayer so that we can capture
    // new responses.
    command_line->AppendSwitchASCII(test::kAutofillServerBehaviorParam,
                                    "ProductionServer");
    AutofillCapturedSitesInteractiveTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    AutofillCapturedSitesInteractiveTest::TearDownOnMainThread();
    EXPECT_TRUE(web_page_replay_server_wrapper_->Stop())
        << "Cannot stop the local Web Page Replay server.";
  }

  WebPageReplayServerWrapper* web_page_replay_server_wrapper() {
    return web_page_replay_server_wrapper_.get();
  }

 private:
  std::unique_ptr<WebPageReplayServerWrapper> web_page_replay_server_wrapper_;
};

// This test is to be run periodically to capture updated Autofill Server
// Predictions. It run the same AutofillCapturedSitesInteractiveTest test suite
// but allows the queries the to Autofill Server to get through WPR and hit the
// live server and captures the new responses in separate .wpr archive files in
// the refresh/ subdirectory of chrome/test/data/autofill/captured_sites
IN_PROC_BROWSER_TEST_P(AutofillCapturedSitesRefresh, Recipe) {
  VLOG(1) << "Recapturing Server Predictions for: " << GetParam().site_name;
  web_page_replay_server_wrapper()->Start(GetParam().refresh_file_path);
  bool test_completed = recipe_replayer()->ReplayTest(
      GetParam().capture_file_path, GetParam().recipe_file_path,
      captured_sites_test_utils::GetCommandFilePath());
  if (!test_completed)
    ADD_FAILURE() << "Full execution was unable to complete.";

  std::vector<testing::AssertionResult> validation_failures =
      recipe_replayer()->GetValidationFailures();
  if (validation_failures.empty()) {
    VLOG(1) << "No Change in Server Predictions for: " << GetParam().site_name;
  } else {
    LOG(INFO) << "There were " << validation_failures.size()
              << " Validation Failure(s). This means Server Predictions "
                 "respones have changed and likely need to be addressed.";
    for (auto& validation_failure : validation_failures)
      ADD_FAILURE() << validation_failure.message();
  }
}

// This test is called with a dynamic list and will be empty during the Password
// run instance, so adding GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST a la
// crbug/1192206
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AutofillCapturedSitesRefresh);
INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillCapturedSitesRefresh,
    testing::ValuesIn(GetCapturedSites(GetReplayFilesRootDirectory())),
    captured_sites_test_utils::GetParamAsString());

}  // namespace
}  // namespace autofill
