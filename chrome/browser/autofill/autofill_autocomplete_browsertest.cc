// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"

namespace autofill {

namespace {

using ::base::UTF8ToUTF16;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

const char kDefaultAutocompleteInputId[] = "n300";
const char kSimpleFormFileName[] = "autocomplete_simple_form.html";

}  // namespace

class AutofillAutocompleteTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    active_browser_ = browser();

    // Don't want Keychain coming up on Mac.
    test::DisableSystemServices(pref_service());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Make sure to close any showing popups prior to tearing down the UI.
    content::WebContents* web_contents =
        active_browser_->tab_strip_model()->GetActiveWebContents();
    active_browser_ = nullptr;
    ContentAutofillDriverFactory::FromWebContents(web_contents)
        ->DriverForFrame(web_contents->GetPrimaryMainFrame())
        ->GetAutofillManager()
        .client()
        .HideAutofillPopup(PopupHidingReason::kTabGone);
    test::ReenableSystemServices();
  }

  // Necessary to avoid flakiness or failure due to input arriving
  // before the first compositor commit.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  // Uses the browser to open the file named |filename| based on the given
  // |disposition|.
  void NavigateToFile(const char* filename,
                      const WindowOpenDisposition& disposition =
                          WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    GURL url = GetURL(filename);
    NavigateParams params(active_browser_, url, ui::PAGE_TRANSITION_LINK);
    params.disposition = disposition;
    ui_test_utils::NavigateToURL(&params);
  }

  // Fills in the default input with |value|, submits the form and waits
  // for the value to have been saved in the DB or skipped, via observers.
  void FillInputAndSubmit(const std::string& value, bool should_skip_save) {
    const char js_format[] =
        "document.getElementById('%s').value = '%s';"
        "document.onclick = function() {"
        "  document.getElementById('testform').submit();"
        "};";

    const std::string js = base::StringPrintf(
        js_format, kDefaultAutocompleteInputId, value.c_str());

    ASSERT_TRUE(content::ExecJs(web_contents(), js));

    // Simulate a mouse click to submit the form because form submissions not
    // triggered by user gestures are ignored.
    TestAutofillManagerWaiter waiter(manager(),
                                     {AutofillManagerEvent::kFormSubmitted});
    content::SimulateMouseClick(
        active_browser_->tab_strip_model()->GetActiveWebContents(), 0,
        blink::WebMouseEvent::Button::kLeft);
    ASSERT_TRUE(waiter.Wait(1));

    if (!should_skip_save) {
      // Wait for data to have been saved in the DB.
      WaitForDBTasks();
    }
  }

  void ReinitializeAutocompleteHistoryManager() {
    autocomplete_history_manager()->Init(GetWebDataService(), pref_service(),
                                         current_profile()->IsOffTheRecord());
  }

  void set_active_browser(Browser* browser) { active_browser_ = browser; }

  AutocompleteHistoryManager* autocomplete_history_manager() {
    return AutocompleteHistoryManagerFactory::GetForProfile(current_profile());
  }

  // Enqueues a RunLoop::QuitClosure using the DB task runner, which executes
  // given tasks sequentially. Then block current execution until the closure
  // has been called.
  void WaitForDBTasks() {
    base::RunLoop run_loop;

    // The quit closure will only be called after all already-queued DB tasks
    // have finished running.
    GetWebDataService()->GetDBTaskRunner()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  AutofillManager& manager() {
    return ContentAutofillDriverFactory::FromWebContents(web_contents())
        ->DriverForFrame(web_contents()->GetPrimaryMainFrame())
        ->GetAutofillManager();
  }

  PrefService* pref_service() { return active_browser_->profile()->GetPrefs(); }

  std::vector<Suggestion> GetAutocompleteSuggestions(
      const std::string& input_name,
      const std::string& prefix) {
    base::MockCallback<SingleFieldFormFiller::OnSuggestionsReturnedCallback>
        callback;
    std::vector<Suggestion> suggestions;
    EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<2>(&suggestions));
    EXPECT_TRUE(autocomplete_history_manager()->OnGetSingleFieldSuggestions(
        AutofillSuggestionTriggerSource::kFormControlElementClicked,
        test::CreateTestFormField(/*label=*/"", input_name, prefix,
                                  FormControlType::kInputText),
        manager().client(), callback.Get(), SuggestionsContext()));

    // Make sure the DB task gets executed.
    WaitForDBTasks();

    return suggestions;
  }

 private:
  GURL GetURL(const std::string& filename) {
    return embedded_test_server()->GetURL("/autofill/" + filename);
  }

  content::WebContents* web_contents() {
    return active_browser_->tab_strip_model()->GetActiveWebContents();
  }

  scoped_refptr<AutofillWebDataService> GetWebDataService() {
    return WebDataServiceFactory::GetAutofillWebDataForProfile(
        current_profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

  Profile* current_profile() { return active_browser_->profile(); }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  raw_ptr<Browser> active_browser_ = nullptr;
};

// Tests that a user can save a simple Autocomplete value.
IN_PROC_BROWSER_TEST_F(AutofillAutocompleteTest, SubmitSimpleValue_Saves) {
  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName);
  FillInputAndSubmit(test_value, /*should_skip_save=*/false);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, prefix),
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  UTF8ToUTF16(test_value), Suggestion::Text::IsPrimary(true))));
}

// Tests that we don't save new autocomplete entries when in Incognito.
IN_PROC_BROWSER_TEST_F(AutofillAutocompleteTest,
                       SubmitSimpleValue_OTR_DoesNotSave) {
  set_active_browser(CreateIncognitoBrowser());

  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName, WindowOpenDisposition::OFF_THE_RECORD);
  FillInputAndSubmit(test_value, /*should_skip_save=*/true);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, ""),
              IsEmpty());
}

// Tests that we don't save new autocomplete entries when Autocomplete was
// disabled by the user.
IN_PROC_BROWSER_TEST_F(AutofillAutocompleteTest,
                       SubmitSimpleValue_Disabled_DoesNotSave) {
  pref_service()->SetBoolean(prefs::kAutofillProfileEnabled, false);
  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName);
  FillInputAndSubmit(test_value, /*should_skip_save=*/true);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, ""),
              IsEmpty());
}

// Tests that initialization of the AutocompleteHistoryManager sets the
// retention policy last version ran preference when the flag is enabled.
IN_PROC_BROWSER_TEST_F(AutofillAutocompleteTest,
                       RetentionPolicy_Init_SavesVersionPref) {
  // Navigate to a file and wait, this will make sure we instantiate
  // AutocompleteHistoryManager.
  NavigateToFile(kSimpleFormFileName);

  // The checkup is executed asynchronously on startup and may not have
  // finished, yet.
  WaitForPrefValue(pref_service(),
                   prefs::kAutocompleteLastVersionRetentionPolicy,
                   base::Value(CHROME_VERSION_MAJOR));

  int saved_version = pref_service()->GetInteger(
      prefs::kAutocompleteLastVersionRetentionPolicy);
  EXPECT_EQ(CHROME_VERSION_MAJOR, saved_version);
}

// Tests that the retention policy cleanup removes an expired entry.
IN_PROC_BROWSER_TEST_F(AutofillAutocompleteTest,
                       RetentionPolicy_RemovesExpiredEntry) {
  // Go back in time, far enough so that we'll expire the entry.
  TestAutofillClock test_clock;
  base::TimeDelta days_delta = 2 * kAutocompleteRetentionPolicyPeriod;
  test_clock.SetNow(AutofillClock::Now() - days_delta);

  // Add an entry.
  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName);
  FillInputAndSubmit(test_value, /*should_skip_save=*/false);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, prefix),
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  UTF8ToUTF16(test_value), Suggestion::Text::IsPrimary(true))));

  // Come back to current time, modify the saved major version and setup our
  // observer.
  test_clock.Advance(days_delta);
  pref_service()->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                             CHROME_VERSION_MAJOR - 1);

  // Trigger the retention policy cleanup (by reinitializing the
  // AutocompleteHistoryManager), and wait for the cleanup to complete.
  ReinitializeAutocompleteHistoryManager();
  WaitForDBTasks();

  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, ""),
              IsEmpty());
}

// Tests that the retention policy cleanup does not remove a valid entry (e.g.
// 20 days old).
IN_PROC_BROWSER_TEST_F(AutofillAutocompleteTest,
                       RetentionPolicy_DoesNot_RemoveValidEntry) {
  // Go back in time, but not far enough so that we'd expire the entry.
  TestAutofillClock test_clock;
  base::TimeDelta days_delta =
      kAutocompleteRetentionPolicyPeriod - base::Days(2);
  test_clock.SetNow(AutofillClock::Now() - days_delta);

  // Add an entry.
  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName);
  FillInputAndSubmit(test_value, /*should_skip_save=*/false);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, prefix),
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  UTF8ToUTF16(test_value), Suggestion::Text::IsPrimary(true))));

  // Come back to current time, modify the saved major version and setup our
  // observer.
  test_clock.Advance(days_delta);
  pref_service()->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                             CHROME_VERSION_MAJOR - 1);

  // Trigger the retention policy cleanup (by reinitializing the
  // AutocompleteHistoryManager), and wait for the cleanup to complete.
  ReinitializeAutocompleteHistoryManager();
  WaitForDBTasks();

  // Verify that the entry is still there.
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, prefix),
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  UTF8ToUTF16(test_value), Suggestion::Text::IsPrimary(true))));
}

}  // namespace autofill
