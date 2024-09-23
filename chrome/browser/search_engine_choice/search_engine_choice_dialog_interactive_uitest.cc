// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kActionButton{"search-engine-choice-app", "#actionButton"};
const DeepQuery kLearnMoreLink{"search-engine-choice-app", "#infoLink"};
const DeepQuery kLearnMoreDialog{"search-engine-choice-app", "#infoDialog"};
const DeepQuery kLearnMoreDialogCloseButton{"search-engine-choice-app",
                                            "#infoDialogButton"};
const DeepQuery kRadioButton = {"search-engine-choice-app", "cr-radio-button"};

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonEnabled);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonDisabled);

}  // namespace

class SearchEngineChoiceDialogInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  auto PressJsButton(const ui::ElementIdentifier web_contents_id,
                     const DeepQuery& button_query) {
    // This can close/navigate the current page, so don't wait for success.
    return ExecuteJsAt(web_contents_id, button_query, "(btn) => btn.click()",
                       ExecuteJsMode::kFireAndForget);
  }

  auto WaitForButtonEnabled(const ui::ElementIdentifier web_contents_id,
                            const DeepQuery& button_query) {
    StateChange button_enabled;
    button_enabled.event = kButtonEnabled;
    button_enabled.where = button_query;
    button_enabled.type = StateChange::Type::kExistsAndConditionTrue;
    button_enabled.test_function = "(btn) => !btn.disabled";
    return WaitForStateChange(web_contents_id, button_enabled);
  }

  auto WaitForButtonDisabled(const ui::ElementIdentifier web_contents_id,
                             const DeepQuery& button_query) {
    StateChange button_disabled;
    button_disabled.event = kButtonDisabled;
    button_disabled.where = button_query;
    button_disabled.type = StateChange::Type::kExistsAndConditionTrue;
    button_disabled.test_function = "(btn) => btn.disabled";
    return WaitForStateChange(web_contents_id, button_disabled);
  }

  // InteractiveBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);

    // Change the country to belgium so that the search engine choice test works
    // as intended.
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");

    // For the item positions to be logged, the variations country has to match
    // the profile country.
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, "be");

    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InteractiveBrowserTest::SetUpInProcessBrowserTestFixture();
    SearchEngineChoiceDialogService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);
  }

  const base::HistogramTester& HistogramTester() const {
    return histogram_tester_;
  }

  const base::UserActionTester& UserActionTester() const {
    return user_action_tester_;
  }

 private:
  base::AutoReset<bool> scoped_chrome_build_override_ =
      SearchEngineChoiceDialogServiceFactory::
          ScopedChromeBuildOverrideForTesting(
              /*force_chrome_build=*/true);
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceDialogInteractiveUiTest,
                       ChooseSearchEngine) {
  SearchEngineChoiceDialogService* search_engine_choice_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(
          browser()->profile());
  int first_search_engine_id =
      search_engine_choice_service->GetSearchEngines().at(0)->prepopulate_id();

  RunTestSequence(InAnyContext(Steps(
      WaitForShow(kSearchEngineChoiceDialogId),
      InstrumentNonTabWebView(kWebContentsId, kSearchEngineChoiceDialogId),
      Do([&] {
        HistogramTester().ExpectUniqueSample(
            search_engines::kSearchEngineChoiceScreenEventsHistogram,
            search_engines::SearchEngineChoiceScreenEvents::
                kChoiceScreenWasDisplayed,
            1);

        EXPECT_EQ(
            UserActionTester().GetActionCount("SearchEngineChoiceScreenShown"),
            1);
      }),
      PressJsButton(kWebContentsId, kLearnMoreLink),
      PressJsButton(kWebContentsId, kLearnMoreDialogCloseButton),
      PressJsButton(kWebContentsId, kActionButton),
      // The button should become disabled because we didn't make a choice.
      WaitForButtonDisabled(kWebContentsId, kActionButton),
      PressJsButton(kWebContentsId, kRadioButton),
      WaitForButtonEnabled(kWebContentsId, kActionButton),
      PressJsButton(kWebContentsId, kActionButton),
      WaitForHide(kSearchEngineChoiceDialogId))));

  HistogramTester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kLearnMoreWasDisplayed,
      1);

  EXPECT_FALSE(search_engine_choice_service->IsShowingDialog(*browser()));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();
  EXPECT_EQ(default_search_engine->prepopulate_id(), first_search_engine_id);

  HistogramTester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet, 1);

  SearchEngineType search_engine_type = default_search_engine->GetEngineType(
      template_url_service->search_terms_data());
  HistogramTester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      search_engine_type, 1);
  HistogramTester().ExpectUniqueSample(
      base::StringPrintf(
          search_engines::
              kSearchEngineChoiceScreenShowedEngineAtHistogramPattern,
          0),
      search_engine_type, 1);

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_FALSE(search_engine_choice_service->IsShowingDialog(*browser()));

  // We expect that the value was recorded at least once because more than one
  // navigation could happen in the background.
  EXPECT_GE(HistogramTester().GetBucketCount(
                search_engines::
                    kSearchEngineChoiceScreenNavigationConditionsHistogram,
                search_engines::SearchEngineChoiceScreenConditions::
                    kAlreadyCompleted),
            1);
  EXPECT_EQ(UserActionTester().GetActionCount("ExpandSearchEngineDescription"),
            1);
}
