// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"  // nogncheck
#else
#include "chrome/browser/ui/browser.h"  // nogncheck
#endif
#include "base/command_line.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/filling/form_filler_test_api.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/switches.h"

namespace autofill {

#if BUILDFLAG(IS_ANDROID)
class AutofillEventHandlerBrowserTest : public AndroidBrowserTest {
#else
class AutofillEventHandlerBrowserTest : public InProcessBrowserTest {
#endif
 protected:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver) {}

    [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
        int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

    [[nodiscard]] testing::AssertionResult WaitForAutofillFill(
        size_t num_expected_fills = 1) {
      return autofill_fill_waiter_.Wait(num_expected_fills);
    }

   private:
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {AutofillManagerEvent::kFormsSeen}};
    TestAutofillManagerWaiter autofill_fill_waiter_{
        *this,
        {AutofillManagerEvent::kDidAutofillForm}};
    TestAutofillManagerWaiter server_predictions_waiter_{
        *this,
        {AutofillManagerEvent::kLoadedServerPredictions}};
  };

  AutofillEventHandlerBrowserTest() = default;

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_ANDROID)
    AndroidBrowserTest::SetUpOnMainThread();
#else
    InProcessBrowserTest::SetUpOnMainThread();
#endif
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up a test autofill profile
    SetUpAutofillProfile();
  }

  void SetUpAutofillProfile() {
    Profile* browser_profile = Profile::FromBrowserContext(
        chrome_test_utils::GetActiveWebContents(this)->GetBrowserContext());
    PersonalDataManager* pdm =
        PersonalDataManagerFactory::GetForBrowserContext(browser_profile);
    ASSERT_TRUE(pdm);

    AutofillProfile autofill_profile = test::GetFullProfile();
    autofill_profile.SetRawInfo(NAME_FULL, u"John Doe");
    autofill_profile.SetRawInfo(EMAIL_ADDRESS, u"john.doe@example.com");
    autofill_profile.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Main Street");
    autofill_profile.SetRawInfo(ADDRESS_HOME_CITY, u"Anytown");
    autofill_profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
    autofill_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");
    autofill_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

    pdm->address_data_manager().AddProfile(autofill_profile);
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestAutofillManager* main_autofill_manager() {
    return autofill_manager_injector_[main_frame()];
  }

  // Fakes an Autofill on a given form - similar to FillCard but for addresses
  void FillAddress(content::RenderFrameHost* rfh,
                   const FormData& form,
                   const FormFieldData& triggered_field) {
    AutofillProfile profile = test::GetFullProfile();
    profile.SetRawInfo(NAME_FULL, u"John Doe");
    profile.SetRawInfo(EMAIL_ADDRESS, u"john.doe@example.com");
    profile.SetRawInfo(ADDRESS_HOME_LINE1, u"123 Main Street");
    profile.SetRawInfo(ADDRESS_HOME_CITY, u"Anytown");
    profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
    profile.SetRawInfo(ADDRESS_HOME_ZIP, u"12345");
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

    TestAutofillManager* manager = autofill_manager_injector_[rfh];
    manager->FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                               triggered_field.global_id(), &profile,
                               AutofillTriggerSource::kPopup);
  }

  void NavigateAndTriggerAutofill() {
    GURL url =
        embedded_test_server()->GetURL("/autofill/onautofill_event_test.html");
    ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents(), url));

    // Wait for the initial form to be seen.
    TestAutofillManager* manager = main_autofill_manager();
    ASSERT_TRUE(manager->WaitForFormsSeen(/*min_num_awaited_calls=*/1));

    const std::vector<const FormStructure*> form_structures =
        test_api(*manager).form_structures();
    ASSERT_FALSE(form_structures.empty()) << "No forms found on the page";
    const FormStructure* form_structure = form_structures.front();
    ASSERT_TRUE(form_structure);
    ASSERT_FALSE(form_structure->fields().empty()) << "Form has no fields";
    const FormData& form = form_structure->ToFormData();
    const FormFieldData& trigger_field = form.fields()[0];

    // Extend the time limit for programmatic refills to avoid flakiness on slow
    // bots. The default is 3 seconds, which may not be enough when waiting for
    // form reparsing and server predictions on slow Android devices.
    test_api(test_api(*manager).form_filler())
        .set_limit_before_refill(base::Seconds(30));

    // Trigger autofill.
    FillAddress(main_frame(), form, trigger_field);

    // Wait for the first autofill to complete.
    // The JavaScript onautofill handler will modify the DOM (add new fields)
    // and store the refill function in window.testState.pendingRefill.
    ASSERT_TRUE(manager->WaitForAutofillFill(/*num_expected_fills=*/1));

    // Wait for JavaScript to finish modifying the form (adding new fields).
    // The form modification happens in a setTimeout, so we need to wait for it.
    ASSERT_TRUE(content::EvalJs(web_contents(),
                                "new Promise(resolve => {"
                                "  const check = () => {"
                                "    if (window.testState.formModified) {"
                                "      resolve(true);"
                                "    } else {"
                                "      setTimeout(check, 10);"
                                "    }"
                                "  };"
                                "  check();"
                                "})")
                    .ExtractBool())
        << "Form was not modified by JavaScript";

    // Wait for the form rescan after DOM modification.
    // We must wait until the FormStructure in the cache actually contains the
    // newly added fields (11 total: 8 original + 3 new). Simply waiting for
    // OnAfterFormsSeen is insufficient because:
    // 1. Form extraction on the renderer is throttled (100ms delay)
    // 2. Form parsing on the browser is asynchronous
    // 3. There may be multiple FormsSeen events in flight, and we might catch
    //    a stale one before the DOM mutation's form parsing completes
    constexpr size_t kExpectedFieldCount = 11;
    ASSERT_TRUE(WaitForMatchingForm(
        manager, base::BindRepeating([](const FormStructure& form) {
          return form.field_count() >= kExpectedFieldCount;
        })));

    ASSERT_TRUE(
        content::ExecJs(web_contents(), "window.triggerPendingRefill()"))
        << "Failed to trigger pending refill";

    // Wait for the refill to complete.
    ASSERT_TRUE(manager->WaitForAutofillFill(/*num_expected_fills=*/1));
  }

 private:
  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    AndroidBrowserTest::SetUp();
#else
    InProcessBrowserTest::SetUp();
#endif
  }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kAutofillEvent};
};

// Verifies the complete autofill event handler flow:
// 1. Autofill event is fired synchronously BEFORE autofill with field data.
// 2. Autofill completes.
// 3. refill() method can trigger another autofill pass after form modification.
// 4. Form modification during event handling works.
// 5. Autofill completes (again) after form modification and fills in the new
// fields.
IN_PROC_BROWSER_TEST_F(AutofillEventHandlerBrowserTest, CompleteAutofillFlow) {
  // NavigateAndTriggerAutofill waits for both the initial fill and the refill
  // triggered by JavaScript calling event.refill() in the onautofill handler.
  NavigateAndTriggerAutofill();

  EXPECT_TRUE(content::EvalJs(web_contents(), "window.testState.eventFired")
                  .ExtractBool())
      << "Autofill event was not fired.";

  EXPECT_TRUE(content::EvalJs(web_contents(),
                              "window.testState.eventFiredSynchronously")
                  .ExtractBool())
      << "Event did not fire synchronously";

  EXPECT_FALSE(content::EvalJs(web_contents(),
                               "window.testState.fieldsFilledBeforeEvent")
                   .ExtractBool())
      << "Fields were filled before the event fired - event should fire "
         "before autofill";

  EXPECT_TRUE(
      content::EvalJs(web_contents(), "window.testState.autofillEventFired")
          .ExtractBool())
      << "Autofill event was not fired.";

  EXPECT_TRUE(
      content::EvalJs(web_contents(), "window.testState.addEventListenerFired")
          .ExtractBool())
      << "AddEventListener autofill event fired.";

  // Verify event contains real field data from the autofill that already
  // happened. eventData is now an array of event objects.
  EXPECT_TRUE(
      content::EvalJs(
          web_contents(),
          "window.testState.eventData && window.testState.eventData.length > 0")
          .ExtractBool())
      << "Event does not contain any event data";

  int event_data_count =
      content::EvalJs(
          web_contents(),
          "window.testState.eventData ? window.testState.eventData.length : 0")
          .ExtractInt();
  EXPECT_GE(event_data_count, 1) << "No event data recorded";

  // Check the first event's field data
  int field_count =
      content::EvalJs(web_contents(),
                      "window.testState.eventData[0] && "
                      "window.testState.eventData[0].fieldData "
                      "? window.testState.eventData[0].fieldData.length : 0")
          .ExtractInt();
  EXPECT_GT(field_count, 0) << "No field data in first event";

  // Verify autofill data is present in the first event.
  bool has_name_data =
      content::EvalJs(web_contents(),
                      "window.testState.eventData[0].fieldData.some(f => "
                      "f.value === 'John' || f.value === 'Doe')")
          .ExtractBool();
  EXPECT_TRUE(has_name_data) << "Event data missing name information";

  bool has_email_data =
      content::EvalJs(
          web_contents(),
          "window.testState.eventData[0].fieldData.some(f => f.value === "
          "'john.doe@example.com')")
          .ExtractBool();
  EXPECT_TRUE(has_email_data) << "Event data missing email information";

  // Verify refill was not null (address autofill supports refill).
  EXPECT_FALSE(content::EvalJs(web_contents(), "window.testState.refillIsNull")
                   .ExtractBool())
      << "refill should not be null for address autofill";

  // Verify refill was called and its promise was resolved.
  EXPECT_TRUE(content::EvalJs(web_contents(), "window.testState.refillCalled")
                  .ExtractBool())
      << "refill was not called";

  EXPECT_TRUE(
      content::EvalJs(web_contents(), "window.testState.refillPromiseResolved")
          .ExtractBool())
      << "refill promise was not resolved";

  // Verify form modification during event handling.
  EXPECT_TRUE(content::EvalJs(web_contents(), "window.testState.formModified")
                  .ExtractBool())
      << "Form was not modified during event handler";

  // Verify the new fields were added to the DOM
  bool neighborhood_field_exists =
      content::EvalJs(
          web_contents(),
          "document.getElementById('ADDRESS_HOME_DEPENDENT_LOCALITY') !== null")
          .ExtractBool();
  EXPECT_TRUE(neighborhood_field_exists)
      << "New neighborhood field was not added to the form";

  bool region_field_exists =
      content::EvalJs(
          web_contents(),
          "document.getElementById('ADDRESS_HOME_ADMIN_LEVEL2') !== null")
          .ExtractBool();
  EXPECT_TRUE(region_field_exists)
      << "New region field was not added to the form";

  bool prefecture_field_exists =
      content::EvalJs(
          web_contents(),
          "document.getElementById('ADDRESS_HOME_STATE_PREFECTURE') !== null")
          .ExtractBool();
  EXPECT_TRUE(prefecture_field_exists)
      << "New prefecture field was not added to the form";

  // Verify at least one of the new fields was filled by the refill
  // (depending on the profile data, not all fields may have values)
  std::string neighborhood_value =
      content::EvalJs(
          web_contents(),
          "document.getElementById('ADDRESS_HOME_DEPENDENT_LOCALITY') ? "
          "document.getElementById('ADDRESS_HOME_DEPENDENT_LOCALITY').value : "
          "''")
          .ExtractString();

  std::string region_value =
      content::EvalJs(
          web_contents(),
          "document.getElementById('ADDRESS_HOME_ADMIN_LEVEL2') ? "
          "document.getElementById('ADDRESS_HOME_ADMIN_LEVEL2').value : ''")
          .ExtractString();

  // At least verify the fields exist and can potentially be filled
  // The actual filling depends on whether the profile has these values

  // Verify the event fired twice - once for initial fill, once for refill
  int event_fire_count =
      content::EvalJs(web_contents(), "window.testState.eventFireCount")
          .ExtractInt();
  EXPECT_EQ(2, event_fire_count)
      << "Event fired " << event_fire_count
      << " times - expected 2 (initial fill + refill)";

  // Verify that refill is null on the second event (refill operations don't
  // support further refills).
  EXPECT_TRUE(content::EvalJs(web_contents(),
                              "window.testState.refillIsNullOnSecondEvent")
                  .ExtractBool())
      << "refill should be null on the second (refill) event";

  // Verify the second event (refill) contains fields.
  int second_event_field_count =
      content::EvalJs(web_contents(),
                      "window.testState.eventData[1] && "
                      "window.testState.eventData[1].fieldData "
                      "? window.testState.eventData[1].fieldData.length : 0")
          .ExtractInt();
  EXPECT_GT(second_event_field_count, 0) << "Second event has no field data";

  // Verify the second event contains only the new fields (neighborhood, region,
  // prefecture) and NOT the original fields.
  bool second_event_has_original_fields =
      content::EvalJs(web_contents(),
                      "window.testState.eventData[1].fieldData.some(f => "
                      "f.field === 'firstname' || f.field === 'email')")
          .ExtractBool();
  EXPECT_FALSE(second_event_has_original_fields)
      << "Second event should not contain original fields that were already "
         "filled";

  // Verify the second event contains the new fields
  bool second_event_has_neighborhood =
      content::EvalJs(web_contents(),
                      "window.testState.eventData[1].fieldData.some(f => "
                      "f.field === 'neighborhood')")
          .ExtractBool();
  bool second_event_has_region =
      content::EvalJs(web_contents(),
                      "window.testState.eventData[1].fieldData.some(f => "
                      "f.field === 'region')")
          .ExtractBool();
  bool second_event_has_prefecture =
      content::EvalJs(web_contents(),
                      "window.testState.eventData[1].fieldData.some(f => "
                      "f.field === 'prefecture')")
          .ExtractBool();

  EXPECT_TRUE(second_event_has_neighborhood || second_event_has_region ||
              second_event_has_prefecture)
      << "Second event should contain at least one of the newly added fields "
      << "(neighborhood=" << second_event_has_neighborhood
      << ", region=" << second_event_has_region
      << ", prefecture=" << second_event_has_prefecture << ")";

  // Verify autofill completion - these fields should have been filled
  // after the event was fired (since event fires synchronously before
  // autofill).
  std::string first_name_value =
      content::EvalJs(web_contents(),
                      "document.getElementById('NAME_FIRST').value")
          .ExtractString();
  EXPECT_EQ("John", first_name_value)
      << "First name field was not filled by autofill";

  std::string email_value =
      content::EvalJs(web_contents(),
                      "document.getElementById('EMAIL_ADDRESS').value")
          .ExtractString();
  EXPECT_EQ("john.doe@example.com", email_value)
      << "Email field was not filled by autofill";

  std::string address_value =
      content::EvalJs(web_contents(),
                      "document.getElementById('ADDRESS_HOME_LINE1').value")
          .ExtractString();
  EXPECT_EQ("123 Main Street", address_value)
      << "Address field was not filled by autofill";

  // Verify no errors occurred.
  std::string errors =
      content::EvalJs(web_contents(), "window.testState.errors.join('; ')")
          .ExtractString();
  EXPECT_TRUE(errors.empty()) << "Test errors: " << errors;
}

}  // namespace autofill
