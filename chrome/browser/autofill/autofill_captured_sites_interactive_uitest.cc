// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

using captured_sites_test_utils::CapturedSiteParams;
using captured_sites_test_utils::GetCapturedSites;

namespace {

const base::TimeDelta autofill_wait_for_action_interval =
    base::TimeDelta::FromSeconds(5);

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
                    const std::vector<std::string> iframe_path,
                    const int attempts,
                    content::RenderFrameHost* frame) override {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(frame);
    AutofillManager* autofill_manager =
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(frame)
            ->autofill_manager();
    autofill_manager->SetTestDelegate(test_delegate());

    int tries = 0;
    while (tries < attempts) {
      tries++;
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
    ServerFieldType type;
    if (!StringToFieldType(field_type, &type)) {
      ADD_FAILURE() << "Unable to recognize autofill field type '" << field_type
                    << "'!";
      return false;
    }

    if (base::StartsWith(field_type, "HTML_TYPE_CREDIT_CARD_",
                         base::CompareCase::INSENSITIVE_ASCII) ||
        base::StartsWith(field_type, "CREDIT_CARD_",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      if (type == autofill::CREDIT_CARD_NAME_FIRST ||
          type == autofill::CREDIT_CARD_NAME_LAST) {
        card_.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                         base::ASCIIToUTF16(""));
      }
      card_.SetRawInfo(type, base::UTF8ToUTF16(field_value));
    } else {
      profile_.SetRawInfo(type, base::UTF8ToUTF16(field_value));
    }

    return true;
  }

  bool SetupAutofillProfile() override {
    AddTestAutofillData(browser(), profile(), credit_card());
    return true;
  }

 protected:
  AutofillCapturedSitesInteractiveTest()
      : profile_(test::GetFullProfile()),
        card_(CreditCard(base::GenerateGUID(), "http://www.example.com")) {
    for (size_t i = NO_SERVER_DATA; i < MAX_VALID_FIELD_TYPE; ++i) {
      ServerFieldType field_type = static_cast<ServerFieldType>(i);
      string_to_field_type_map_[AutofillType(field_type).ToString()] =
          field_type;
    }

    for (size_t i = HTML_TYPE_UNSPECIFIED; i < HTML_TYPE_UNRECOGNIZED; ++i) {
      AutofillType field_type(static_cast<HtmlFieldType>(i), HTML_MODE_NONE);
      string_to_field_type_map_[field_type.ToString()] =
          field_type.GetStorableType();
    }

    // Initialize the credit card with default values, in case the test recipe
    // file does not contain pre-saved credit card info.
    test::SetCreditCardInfo(&card_, "Buddy Holly", "5187654321098765", "10",
                            "2998", "1");
  }

  ~AutofillCapturedSitesInteractiveTest() override {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AutofillUiTest::SetUpOnMainThread();
    recipe_replayer_ =
        std::make_unique<captured_sites_test_utils::TestRecipeReplayer>(
            browser(), this);
    recipe_replayer()->Setup();

    SetServerUrlLoader(std::make_unique<test::ServerUrlLoader>(
        std::make_unique<test::ServerCacheReplayer>(
            GetParam().capture_file_path,
            test::ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                test::ServerCacheReplayer::kOptionSplitRequestsByForm,
            base::FeatureList::IsEnabled(features::kAutofillUseApi)
                ? test::AutofillServerType::kApi
                : test::AutofillServerType::kLegacy)));
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the autofill show typed prediction feature. When active this
    // feature forces input elements on a form to display their autofill type
    // prediction. Test will check this attribute on all the relevant input
    // elements in a form to determine if the form is ready for interaction.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillShowTypePredictions,
                              features::kAutofillUseApi},
        /*disabled_features=*/{features::kAutofillCacheQueryResponses});
    command_line->AppendSwitch(switches::kShowAutofillTypePredictions);
    command_line->AppendSwitchASCII(::switches::kForceFieldTrials,
                                    "AutofillFieldMetadata/Enabled/");

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

  const CreditCard credit_card() { return card_; }

  const AutofillProfile profile() { return profile_; }

 private:
  bool ShowAutofillSuggestion(const std::string& target_element_xpath,
                              const std::vector<std::string> iframe_path,
                              content::RenderFrameHost* frame) {
    // First, automation should focus on the frame containg the autofill form.
    // Doing so ensures that Chrome scrolls the element into view if the
    // element is off the page.
    if (!captured_sites_test_utils::TestRecipeReplayer::PlaceFocusOnElement(
            target_element_xpath, iframe_path, frame))
      return false;

    gfx::Rect rect;
    if (!captured_sites_test_utils::TestRecipeReplayer::
            GetBoundingRectOfTargetElement(target_element_xpath, iframe_path,
                                           frame, &rect))
      return false;

    test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionShown},
                                     autofill_wait_for_action_interval);
    if (!captured_sites_test_utils::TestRecipeReplayer::
            SimulateLeftMouseClickAt(rect.CenterPoint(), frame))
      return false;

    return test_delegate()->Wait();
  }

  bool StringToFieldType(const std::string& str, ServerFieldType* type) {
    if (string_to_field_type_map_.count(str) == 0)
      return false;
    *type = string_to_field_type_map_[str];
    return true;
  }

  AutofillProfile profile_;
  CreditCard card_;
  std::unique_ptr<captured_sites_test_utils::TestRecipeReplayer>
      recipe_replayer_;
  std::map<const std::string, ServerFieldType> string_to_field_type_map_;

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<test::ServerUrlLoader> server_url_loader_;
};

IN_PROC_BROWSER_TEST_P(AutofillCapturedSitesInteractiveTest, Recipe) {
  // Prints the path of the test to be executed.
  VLOG(1) << GetParam().site_name;

  base::FilePath src_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));

  bool test_completed = recipe_replayer()->ReplayTest(
      GetParam().capture_file_path, GetParam().recipe_file_path);
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
INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillCapturedSitesInteractiveTest,
    testing::ValuesIn(GetCapturedSites(GetReplayFilesRootDirectory())),
    captured_sites_test_utils::GetParamAsString());
}  // namespace autofill
