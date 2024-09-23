// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <unordered_map>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace policy {
namespace {

using ::testing::AssertionResult;
using ::testing::Contains;
using ::testing::Field;
using ::testing::IsEmpty;

const char kAutofillTestPageURL[] = "/autofill/autofill_address_enabled.html";

class AutofillPolicyTest : public PolicyTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    autofill::WaitForPersonalDataManagerToBeLoaded(browser()->profile());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  autofill::PersonalDataManager* personal_data_manager() {
    return autofill::PersonalDataManagerFactory::GetForBrowserContext(
        browser()->profile());
  }

  [[nodiscard]] testing::AssertionResult ImportAddress() {
    if (personal_data_manager()->address_data_manager().GetProfiles().size() !=
        0u) {
      return testing::AssertionFailure() << "Should be empty profile.";
    }
    autofill::AddTestProfile(browser()->profile(),
                             autofill::test::GetFullProfile());
    expected_suggestions_["name"] = u"John H. Doe";
    expected_suggestions_["street-address"] = u"666 Erebus St., Apt 8";
    expected_suggestions_["postal-code"] = u"91111";
    expected_suggestions_["city"] = u"Elysium";
    expected_suggestions_["phone"] = u"+1 650-211-1111";
    expected_suggestions_["email"] = u"johndoe@hades.com";
    return personal_data_manager()
                       ->address_data_manager()
                       .GetProfiles()
                       .size() == 1u
               ? testing::AssertionSuccess()
               : testing::AssertionFailure() << "Should be one profile.";
  }

  std::unordered_map<std::string, std::u16string> GetExpectedSuggestions() {
    return expected_suggestions_;
  }

  [[nodiscard]] testing::AssertionResult NavigateToTestPage() {
    if (!(ui_test_utils::NavigateToURL(
            browser(), embedded_test_server()->GetURL(kAutofillTestPageURL)))) {
      return testing::AssertionFailure();
    }
    // Wait for the test page to be rendered to receive clicks.
    content::MainThreadFrameObserver frame_observer(
        GetWebContents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
    frame_observer.Wait();
    return testing::AssertionSuccess();
  }

 protected:
  class TestAutofillManager : public autofill::BrowserAutofillManager {
   public:
    explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
        : autofill::BrowserAutofillManager(driver, "en-US") {}

    [[nodiscard]] AssertionResult WaitForFormsSeen() {
      return forms_seen_waiter_.Wait();
    }

    // The test can not wait for autofill popup to show, because when
    // autofill gets disabled, the test will hang there. An alternative is to
    // have a timeout, but it could be flaky on bots with different specs.
    // Hence the test checks the OnAskForValues event, if this event got
    // fired, Autofill popup should have appeared, otherwise it is disabled by
    // policy.
    [[nodiscard]] AssertionResult WaitForAskForValuesToFill() {
      return ask_for_value_to_fill_waiter_.Wait();
    }

   private:
    autofill::TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {autofill::AutofillManagerEvent::kFormsSeen}};
    autofill::TestAutofillManagerWaiter ask_for_value_to_fill_waiter_{
        *this,
        {autofill::AutofillManagerEvent::kAskForValuesToFill}};
  };

  class TestAutofillClient : public autofill::ChromeAutofillClient {
   public:
    explicit TestAutofillClient(content::WebContents* web_contents)
        : autofill::ChromeAutofillClient(web_contents) {}

    SuggestionUiSessionId ShowAutofillSuggestions(
        const autofill::AutofillClient::PopupOpenArgs& open_args,
        base::WeakPtr<autofill::AutofillSuggestionDelegate> delegate) override {
      suggestions_ = open_args.suggestions;
      return autofill::ChromeAutofillClient::ShowAutofillSuggestions(open_args,
                                                                     delegate);
    }

    const std::vector<autofill::Suggestion>& suggestions() const {
      return suggestions_;
    }

    void ResetSuggestions() { suggestions_ = {}; }

   private:
    std::vector<autofill::Suggestion> suggestions_;
  };

  TestAutofillClient* autofill_client() {
    return autofill_client_injector_[GetWebContents()];
  }

  TestAutofillManager* autofill_manager() {
    return autofill_manager_injector_[GetWebContents()];
  }

 private:
  autofill::TestAutofillClientInjector<TestAutofillClient>
      autofill_client_injector_;

  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;

  std::unordered_map<std::string, std::u16string> expected_suggestions_;
};

IN_PROC_BROWSER_TEST_F(AutofillPolicyTest, AutofillEnabledByPolicy) {
  ASSERT_TRUE(ImportAddress());
  PolicyMap policies;
  SetPolicy(&policies, key::kAutofillAddressEnabled, base::Value(true));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(NavigateToTestPage());
  EXPECT_TRUE(autofill_manager()->WaitForFormsSeen());
  for (const auto& [element, expectation] : GetExpectedSuggestions()) {
    SCOPED_TRACE(testing::Message() << "element = " << element
                                    << ", expectation = " << expectation);
    content::SimulateMouseClickOrTapElementWithId(GetWebContents(), element);
    EXPECT_TRUE(autofill_manager()->WaitForAskForValuesToFill());
    // Showing the Autofill Popup is an asynchronous task.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return !autofill_client()->suggestions().empty();
    })) << "Showing the Autofill Popup timed out.";
    // There may be more suggestions, but the first one in the vector
    // should be the expected and shown in the popup.
    EXPECT_THAT(autofill_client()->suggestions(),
                Contains(Field(
                    &autofill::Suggestion::main_text,
                    Field(&autofill::Suggestion::Text::value, expectation))));
    autofill_client()->ResetSuggestions();
  }
}

IN_PROC_BROWSER_TEST_F(AutofillPolicyTest, AutofillDisabledByPolicy) {
  ASSERT_TRUE(ImportAddress());
  PolicyMap policies;
  SetPolicy(&policies, key::kAutofillAddressEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(NavigateToTestPage());
  EXPECT_TRUE(autofill_manager()->WaitForFormsSeen());
  for (const auto& [element, expectation] : GetExpectedSuggestions()) {
    SCOPED_TRACE(testing::Message() << "element = " << element
                                    << ", expectation = " << expectation);
    content::SimulateMouseClickOrTapElementWithId(GetWebContents(), element);
    EXPECT_TRUE(autofill_manager()->WaitForAskForValuesToFill());
    // Showing the Autofill Popup is an asynchronous task.
    base::RunLoop().RunUntilIdle();
    EXPECT_THAT(autofill_client()->suggestions(), IsEmpty());
    autofill_client()->ResetSuggestions();
  }
}

}  // namespace
}  // namespace policy
