// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autocomplete_history_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
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
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace autofill {

namespace {

using ::base::UTF8ToUTF16;
using ::testing::_;
using ::testing::AssertionResult;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

const char kDefaultAutocompleteInputId[] = "n300";
const char kSimpleFormFileName[] = "autocomplete_simple_form.html";

class AutocompleteTest : public InProcessBrowserTest {
 public:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver, "en-US") {}

    TestAutofillManagerWaiter& text_field_change_waiter() {
      return text_field_change_waiter_;
    }
    TestAutofillManagerWaiter& forms_seen_waiter() {
      return forms_seen_waiter_;
    }
    TestAutofillManagerWaiter& form_submitted_waiter() {
      return form_submitted_waiter_;
    }

   private:
    TestAutofillManagerWaiter text_field_change_waiter_{
        *this,
        {AutofillManagerEvent::kTextFieldDidChange}};
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {AutofillManagerEvent::kFormsSeen}};
    TestAutofillManagerWaiter form_submitted_waiter_{
        *this,
        {AutofillManagerEvent::kFormSubmitted}};
  };

  TestAutofillManager* autofill_manager() {
    return autofill_manager(web_contents()->GetPrimaryMainFrame());
  }

  TestAutofillManager* autofill_manager(content::RenderFrameHost* rfh) {
    return autofill_manager_injector_[rfh];
  }

 protected:
  void SetUpOnMainThread() override {
    active_browser_ = browser();

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // RunUntilIdle() is necessary because otherwise, under the hood
    // PasswordFormManager::OnFetchComplete() callback is run after this test is
    // destroyed meaning that OsCryptImpl will be used instead of OsCryptMocker,
    // causing this test to fail.
    base::RunLoop().RunUntilIdle();
    // Make sure to close any showing popups prior to tearing down the UI.
    ContentAutofillDriver::GetForRenderFrameHost(
        web_contents()->GetPrimaryMainFrame())
        ->GetAutofillManager()
        .client()
        .HideAutofillSuggestions(SuggestionHidingReason::kTabGone);
    active_browser_ = nullptr;
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
    GURL url =
        embedded_test_server()->GetURL(base::StrCat({"/autofill/", filename}));
    NavigateParams params(active_browser_, url, ui::PAGE_TRANSITION_LINK);
    params.disposition = disposition;
    ui_test_utils::NavigateToURL(&params);
  }

  // Fills in the default input with |value|, submits the form and waits
  // for the value to have been saved in the DB or skipped, via observers.
  void FillInputAndSubmit(const std::string& value) {
    const std::string js =
        base::StringPrintf(R"(document.getElementById('%s').focus();
           document.onclick = function() {
             document.getElementById('testform').submit();
           };)",
                           kDefaultAutocompleteInputId);
    ASSERT_TRUE(content::ExecJs(web_contents(), js));

    for (const char c : value) {
      ui::DomKey key = ui::DomKey::FromCharacter(c);
      ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
      ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
      content::SimulateKeyPress(web_contents(), key, code, key_code, false,
                                false, false, false);
      ASSERT_TRUE(autofill_manager()->text_field_change_waiter().Wait(1));
    }

    // Simulate a mouse click to submit the form because form submissions not
    // triggered by user gestures are ignored.
    content::SimulateMouseClick(
        active_browser_->tab_strip_model()->GetActiveWebContents(), 0,
        blink::WebMouseEvent::Button::kLeft);
    ASSERT_TRUE(autofill_manager()->form_submitted_waiter().Wait(1));

    // Wait for data to have been saved in the DB.
    WaitForPendingDBTasks(*GetWebDataService());
  }

  // The retention policy clean-up is run once per major version during
  // initialization. This function triggers it by reinitializing the
  // `autocomplete_history_manager()` and waiting for the cleanup to complete.
  void TriggerRetentionPolicyCleanup() {
    pref_service()->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                               CHROME_VERSION_MAJOR - 1);
    autocomplete_history_manager()->Init(
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            current_profile(), ServiceAccessType::EXPLICIT_ACCESS),
        pref_service(), current_profile()->IsOffTheRecord());
    WaitForPendingDBTasks(*GetWebDataService());
  }

  void set_active_browser(Browser* browser) { active_browser_ = browser; }

  AutocompleteHistoryManager* autocomplete_history_manager() {
    return AutocompleteHistoryManagerFactory::GetForProfile(current_profile());
  }

  PrefService* pref_service() { return active_browser_->profile()->GetPrefs(); }

  std::vector<Suggestion> GetAutocompleteSuggestions(
      const std::string& input_name,
      const std::string& prefix) {
    base::MockCallback<SingleFieldFormFiller::OnSuggestionsReturnedCallback>
        callback;
    std::vector<Suggestion> suggestions;
    EXPECT_CALL(callback, Run).WillOnce(testing::SaveArg<1>(&suggestions));
    EXPECT_TRUE(autocomplete_history_manager()->OnGetSingleFieldSuggestions(
        /*form_structure=*/nullptr,
        test::CreateTestFormField(/*label=*/"", input_name, prefix,
                                  FormControlType::kInputText),
        /*autofill_field=*/nullptr, autofill_manager()->client(),
        callback.Get()));

    // Make sure the DB task gets executed.
    WaitForPendingDBTasks(*GetWebDataService());

    return suggestions;
  }

  scoped_refptr<AutofillWebDataService> GetWebDataService() {
    return WebDataServiceFactory::GetAutofillWebDataForProfile(
        current_profile(), ServiceAccessType::EXPLICIT_ACCESS);
  }

 private:
  content::WebContents* web_contents() {
    return active_browser_->tab_strip_model()->GetActiveWebContents();
  }

  Profile* current_profile() { return active_browser_->profile(); }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
  raw_ptr<Browser> active_browser_ = nullptr;
};

// Tests that a user can save a simple Autocomplete value.
IN_PROC_BROWSER_TEST_F(AutocompleteTest, SubmitSimpleValue_Saves) {
  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName);
  ASSERT_TRUE(autofill_manager()->forms_seen_waiter().Wait(1));
  FillInputAndSubmit(test_value);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, prefix),
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  UTF8ToUTF16(test_value), Suggestion::Text::IsPrimary(true))));
}

// Tests that we don't save new autocomplete entries when in Incognito.
IN_PROC_BROWSER_TEST_F(AutocompleteTest,
                       SubmitSimpleValue_OTR_DoesNotSave) {
  set_active_browser(CreateIncognitoBrowser());

  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName, WindowOpenDisposition::OFF_THE_RECORD);
  ASSERT_TRUE(autofill_manager()->forms_seen_waiter().Wait(1));
  FillInputAndSubmit(test_value);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, ""),
              IsEmpty());
}

// Tests that we don't save new autocomplete entries when Autocomplete was
// disabled by the user.
IN_PROC_BROWSER_TEST_F(AutocompleteTest,
                       SubmitSimpleValue_Disabled_DoesNotSave) {
  pref_service()->SetBoolean(prefs::kAutofillProfileEnabled, false);
  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName);
  ASSERT_TRUE(autofill_manager()->forms_seen_waiter().Wait(1));
  FillInputAndSubmit(test_value);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, ""),
              IsEmpty());
}

// Tests that initialization of the AutocompleteHistoryManager sets the
// retention policy last version ran preference when the flag is enabled.
IN_PROC_BROWSER_TEST_F(AutocompleteTest,
                       RetentionPolicy_Init_SavesVersionPref) {
  // Navigate to a file and wait, this will make sure we instantiate
  // AutocompleteHistoryManager.
  NavigateToFile(kSimpleFormFileName);
  ASSERT_TRUE(autofill_manager()->forms_seen_waiter().Wait(1));

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
IN_PROC_BROWSER_TEST_F(AutocompleteTest,
                       RetentionPolicy_RemovesExpiredEntry) {
  TestAutofillClock test_clock(AutofillClock::Now());

  // Add an entry.
  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName);
  ASSERT_TRUE(autofill_manager()->forms_seen_waiter().Wait(1));
  FillInputAndSubmit(test_value);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, prefix),
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  UTF8ToUTF16(test_value), Suggestion::Text::IsPrimary(true))));

  // Advance time to expire the entry.
  test_clock.Advance(2 * kAutocompleteRetentionPolicyPeriod);

  TriggerRetentionPolicyCleanup();

  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, ""),
              IsEmpty());
}

// Tests that the retention policy cleanup does not remove a valid entry (e.g.
// 20 days old).
IN_PROC_BROWSER_TEST_F(AutocompleteTest,
                       RetentionPolicy_DoesNot_RemoveValidEntry) {
  TestAutofillClock test_clock(AutofillClock::Now());

  // Add an entry.
  std::string prefix = "Some";
  std::string test_value = "SomeName!";
  NavigateToFile(kSimpleFormFileName);
  ASSERT_TRUE(autofill_manager()->forms_seen_waiter().Wait(1));
  FillInputAndSubmit(test_value);
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, prefix),
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  UTF8ToUTF16(test_value), Suggestion::Text::IsPrimary(true))));

  // Advance time by less than `kAutocompleteRetentionPolicyPeriod`.
  test_clock.Advance(kAutocompleteRetentionPolicyPeriod - base::Days(2));

  TriggerRetentionPolicyCleanup();

  // Verify that the entry is still there.
  EXPECT_THAT(GetAutocompleteSuggestions(kDefaultAutocompleteInputId, prefix),
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  UTF8ToUTF16(test_value), Suggestion::Text::IsPrimary(true))));
}

}  // namespace

}  // namespace autofill
