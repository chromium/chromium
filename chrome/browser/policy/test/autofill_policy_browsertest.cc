// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <unordered_map>

#include "base/test/bind.h"
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
const char kAutofillTestPageURL[] = "/autofill/autofill_address_enabled.html";
}  // namespace

class AutofillPolicyTest : public PolicyTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Don't want Keychain coming up on Mac.
    autofill::test::DisableSystemServices(browser()->profile()->GetPrefs());

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    autofill::WaitForPersonalDataManagerToBeLoaded(browser()->profile());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  autofill::PersonalDataManager* personal_data_manager() {
    return autofill::PersonalDataManagerFactory::GetForProfile(
        browser()->profile());
  }

  [[nodiscard]] testing::AssertionResult ImportAddress() {
    if (personal_data_manager()->GetProfiles().size() != 0u) {
      return testing::AssertionFailure() << "Should be empty profile.";
    }
    autofill::PdmChangeWaiter observer(browser()->profile());
    personal_data_manager()->AddProfile(autofill::test::GetFullProfile());
    observer.Wait();
    expected_suggestions_["name"] = u"John H. Doe";
    expected_suggestions_["street-address"] = u"666 Erebus St., Apt 8";
    expected_suggestions_["postal-code"] = u"91111";
    expected_suggestions_["city"] = u"Elysium";
    expected_suggestions_["phone"] = u"+1 650-211-1111";
    expected_suggestions_["email"] = u"johndoe@hades.com";
    return personal_data_manager()->GetProfiles().size() == 1u
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
    TestAutofillManager(autofill::ContentAutofillDriver* driver,
                        autofill::AutofillClient* client)
        : autofill::BrowserAutofillManager(driver, client, "en-US") {}

    void WaitForAskForValuesToFill() {
      base::RunLoop run_loop;
      run_loop_ = &run_loop;
      run_loop_->Run();
    }

    // The test can not wait for autofill popup to show, because when
    // autofill gets disabled, the test will hang there. An alternative is to
    // have a timeout, but it could be flaky on bots with different specs.
    // Hence the test checks the OnAskForValues event, if this event got
    // fired, Autofill popup should have appeared, otherwise it is disabled by
    // policy.
    void OnAskForValuesToFill(
        const autofill::FormData& form,
        const autofill::FormFieldData& field,
        const gfx::RectF& bounding_box,
        autofill::AutofillSuggestionTriggerSource trigger_source) override {
      autofill::TestAutofillManagerWaiter waiter(
          *this, {autofill::AutofillManagerEvent::kAskForValuesToFill});
      autofill::AutofillManager::OnAskForValuesToFill(form, field, bounding_box,
                                                      trigger_source);
      ASSERT_TRUE(waiter.Wait());
      if (run_loop_) {
        run_loop_->Quit();
        run_loop_ = nullptr;
      }
    }

    const autofill::FormStructure* WaitForFormWithNFields(size_t n) {
      return WaitForMatchingForm(
          this,
          base::BindLambdaForTesting([n](const autofill::FormStructure& form) {
            return form.active_field_count() == n;
          }));
    }

   private:
    raw_ptr<base::RunLoop> run_loop_ = nullptr;
  };

  class TestAutofillClient : public autofill::ChromeAutofillClient {
   public:
    explicit TestAutofillClient(content::WebContents* web_contents)
        : autofill::ChromeAutofillClient(web_contents) {}

    void ShowAutofillPopup(
        const autofill::AutofillClient::PopupOpenArgs& open_args,
        base::WeakPtr<autofill::AutofillPopupDelegate> delegate) override {
      autofill::ChromeAutofillClient::ShowAutofillPopup(open_args, delegate);
      popup_shown_ = true;
    }

    bool HasShownAutofillPopup() const { return popup_shown_; }

    void ResetPopupShown() { popup_shown_ = false; }

   private:
    bool popup_shown_ = false;
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
  EXPECT_TRUE(autofill_manager()->WaitForFormWithNFields(6u));
  for (const auto& [element, expectation] : GetExpectedSuggestions()) {
    content::SimulateMouseClickOrTapElementWithId(GetWebContents(), element);
    autofill_manager()->WaitForAskForValuesToFill();
    EXPECT_TRUE(autofill_client()->HasShownAutofillPopup());
    // There may be more suggestions, but the first one in the vector
    // should be the expected and shown in the popup.
    std::vector<autofill::Suggestion> suggestions =
        autofill_client()->GetPopupSuggestions();
    ASSERT_GE(suggestions.size(), 1u);
    EXPECT_EQ(expectation, suggestions[0].main_text.value);
    autofill_client()->ResetPopupShown();
  }
}

IN_PROC_BROWSER_TEST_F(AutofillPolicyTest, AutofillDisabledByPolicy) {
  ASSERT_TRUE(ImportAddress());
  PolicyMap policies;
  SetPolicy(&policies, key::kAutofillAddressEnabled, base::Value(false));
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(NavigateToTestPage());
  EXPECT_TRUE(autofill_manager()->WaitForFormWithNFields(6u));
  for (const auto& [element, _] : GetExpectedSuggestions()) {
    content::SimulateMouseClickOrTapElementWithId(GetWebContents(), element);
    autofill_manager()->WaitForAskForValuesToFill();
    EXPECT_FALSE(autofill_client()->HasShownAutofillPopup());
    EXPECT_EQ(autofill_client()->GetPopupSuggestions().size(), 0u);
  }
}

}  // namespace policy
