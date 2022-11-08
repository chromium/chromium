// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/automated_tests/cache_replayer.h"
#include "chrome/browser/autofill/captured_sites_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/variations/variations_switches.h"
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

namespace {

const base::TimeDelta autofill_wait_for_action_interval = base::Seconds(5);

base::FilePath GetReplayFilesRootDirectory() {
  base::FilePath src_dir;
  if (base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    return src_dir.AppendASCII("chrome")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("autofill")
        .AppendASCII("captured_sites");
  } else {
    src_dir.clear();
    return src_dir;
  }
}

}  // namespace

namespace autofill {

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
                    content::RenderFrameHost* frame) override {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(frame);
    auto* autofill_manager = static_cast<BrowserAutofillManager*>(
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(frame->GetMainFrame())
            ->autofill_manager());
    autofill_manager->SetTestDelegate(test_delegate());

    int tries = 0;
    while (tries < attempts) {
      tries++;

      // Translation bubbles and address-save prompts and others may overlap
      // with and thus prevent the Autofill popup, so we preemptively close all
      // bubbles.
      translate::test_utils::CloseCurrentBubble(browser());
      TryToCloseAllPrompts(web_contents);

      autofill_manager->client()->HideAutofillPopup(
          autofill::PopupHidingReason::kViewDestroyed);

      if (!ShowAutofillSuggestion(focus_element_css_selector, iframe_path,
                                  frame)) {
        LOG(WARNING) << "Failed to bring up the autofill suggestion drop down.";
        continue;
      }

      // Press the down key to highlight the first choice in the autofill
      // suggestion drop down.
      test_delegate()->SetExpectations({ObservedUiEvents::kPreviewFormData},
                                       autofill_wait_for_action_interval);
      SendKeyToPopup(frame, ui::DomKey::ARROW_DOWN);
      if (!test_delegate()->Wait()) {
        LOG(WARNING) << "Failed to select an option from the "
                     << "autofill suggestion drop down.";
        continue;
      }

      // Press the enter key to invoke autofill using the first suggestion.
      test_delegate()->SetExpectations({ObservedUiEvents::kFormDataFilled},
                                       autofill_wait_for_action_interval);
      SendKeyToPopup(frame, ui::DomKey::ENTER);
      if (!test_delegate()->Wait()) {
        LOG(WARNING) << "Failed to fill the form.";
        continue;
      }

      return true;
    }

    autofill_manager->client()->HideAutofillPopup(
        autofill::PopupHidingReason::kViewDestroyed);
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
  AutofillCapturedSitesInteractiveTest() = default;

  ~AutofillCapturedSitesInteractiveTest() override {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AutofillUiTest::SetUpOnMainThread();
    if (base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
      test_delegate()->SetIgnoreBackToBackMessages(
          ObservedUiEvents::kPreviewFormData, true);
      test_delegate()->SetIgnoreBackToBackMessages(
          ObservedUiEvents::kFormDataFilled, true);
    }
    recipe_replayer_ =
        std::make_unique<captured_sites_test_utils::TestRecipeReplayer>(
            browser(), this);
    recipe_replayer()->Setup();

    SetServerUrlLoader(std::make_unique<test::ServerUrlLoader>(
        std::make_unique<test::ServerCacheReplayer>(
            GetParam().capture_file_path,
            test::ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                test::ServerCacheReplayer::kOptionSplitRequestsByForm)));
  }

  void TearDownOnMainThread() override {
    recipe_replayer()->Cleanup();
    // Need to delete the URL loader and its underlying interceptor on the main
    // thread. Will result in a fatal crash otherwise. The pointer has its
    // memory cleaned up twice: first time in that single thread, a second time
    // when the fixture's destructor is called, which will have no effect since
    // the raw pointer will be nullptr.
    server_url_loader_.reset(nullptr);
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
        /*enabled_features=*/{{features::kAutofillAcrossIframes, {}},
                              {features::kAutofillShowTypePredictions, {}},
                              {features::kAutofillParsingPatternProvider,
                               {{"prediction_source", "nextgen"}}}},
        /*disabled_features=*/{});
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, "us");
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
  bool ShowAutofillSuggestion(const std::string& target_element_xpath,
                              const std::vector<std::string> iframe_path,
                              content::RenderFrameHost* frame) {
    // First, automation should focus on the frame containg the autofill form.
    // Doing so ensures that Chrome scrolls the element into view if the
    // element is off the page.
    test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionShown},
                                     autofill_wait_for_action_interval);
    if (!captured_sites_test_utils::TestRecipeReplayer::PlaceFocusOnElement(
            target_element_xpath, iframe_path, frame)) {
      return false;
    }

    gfx::Rect rect;
    if (!captured_sites_test_utils::TestRecipeReplayer::
            GetBoundingRectOfTargetElement(target_element_xpath, iframe_path,
                                           frame, &rect)) {
      return false;
    }

    test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionShown},
                                     autofill_wait_for_action_interval);
    if (!captured_sites_test_utils::TestRecipeReplayer::
            SimulateLeftMouseClickAt(rect.CenterPoint(), frame))
      return false;

    return test_delegate()->Wait();
  }

  std::unique_ptr<captured_sites_test_utils::TestRecipeReplayer>
      recipe_replayer_;
  std::unique_ptr<captured_sites_test_utils::ProfileDataController>
      profile_controller_ =
          std::make_unique<captured_sites_test_utils::ProfileDataController>();

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<test::ServerUrlLoader> server_url_loader_;
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
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));

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

}  // namespace autofill
