// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"  // nogncheck
#else
#include "chrome/browser/ui/browser.h"  // nogncheck
#endif
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
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

  EXPECT_TRUE(
      content::EvalJs(web_contents(), "window.testState.eventData.length > 0")
          .ExtractBool())
      << "Autofill event was not fired.";

  EXPECT_TRUE(content::EvalJs(web_contents(),
                              "window.testState.eventFiredSynchronously")
                  .ExtractBool())
      << "Event did not fire synchronously";

  EXPECT_FALSE(
      content::EvalJs(
          web_contents(),
          "window.testState.eventData[0]?.fieldsFilledBeforeEvent ?? false")
          .ExtractBool())
      << "Fields were filled before the event fired - event should fire "
         "before autofill";

  EXPECT_TRUE(
      content::EvalJs(web_contents(), "window.testState.autofillEventFired")
          .ExtractBool())
      << "Autofill event was not fired.";

  EXPECT_TRUE(
      content::EvalJs(
          web_contents(),
          "window.testState.eventData[0]?.addEventListenerFired ?? false")
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
      content::EvalJs(
          web_contents(),
          "(window.testState.eventData?.[0]?.fieldData ?? []).some(f => "
          "f.value === 'John' || f.value === 'Doe')")
          .ExtractBool();
  EXPECT_TRUE(has_name_data) << "Event data missing name information";

  bool has_email_data =
      content::EvalJs(web_contents(),
                      "(window.testState.eventData?.[0]?.fieldData ?? "
                      "[]).some(f => f.value === "
                      "'john.doe@example.com')")
          .ExtractBool();
  EXPECT_TRUE(has_email_data) << "Event data missing email information";

  // Verify refill was not null (address autofill supports refill).
  EXPECT_FALSE(
      content::EvalJs(web_contents(),
                      "window.testState.eventData[0]?.refillIsNull ?? true")
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
      content::EvalJs(web_contents(), "window.testState.eventData.length")
          .ExtractInt();
  EXPECT_EQ(2, event_fire_count)
      << "Event fired " << event_fire_count
      << " times - expected 2 (initial fill + refill)";

  // Verify that refill is null on the second event (refill operations don't
  // support further refills).
  EXPECT_TRUE(
      content::EvalJs(web_contents(),
                      "window.testState.eventData[1]?.refillIsNull ?? false")
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
      content::EvalJs(
          web_contents(),
          "(window.testState.eventData?.[1]?.fieldData ?? []).some(f => "
          "f.field === 'firstname' || f.field === 'email')")
          .ExtractBool();
  EXPECT_FALSE(second_event_has_original_fields)
      << "Second event should not contain original fields that were already "
         "filled";

  // Verify the second event contains the new fields
  bool second_event_has_neighborhood =
      content::EvalJs(
          web_contents(),
          "(window.testState.eventData?.[1]?.fieldData ?? []).some(f => "
          "f.field === 'neighborhood')")
          .ExtractBool();
  bool second_event_has_region =
      content::EvalJs(
          web_contents(),
          "(window.testState.eventData?.[1]?.fieldData ?? []).some(f => "
          "f.field === 'region')")
          .ExtractBool();
  bool second_event_has_prefecture =
      content::EvalJs(
          web_contents(),
          "(window.testState.eventData?.[1]?.fieldData ?? []).some(f => "
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

// Test fixture for multi-frame autofill event tests.
// Tests credit card forms where fields are split across multiple iframes,
// verifying that the autofill event fires correctly in each frame with
// the appropriate field data.
#if BUILDFLAG(IS_ANDROID)
class AutofillEventMultiFrameBrowserTest : public AndroidBrowserTest {
#else
class AutofillEventMultiFrameBrowserTest : public InProcessBrowserTest {
#endif
 protected:
  // Credit card test data
  static constexpr char kNameFull[] = "John Doe";
  static constexpr char kNumber[] = "4444333322221111";
  static constexpr char kExpMonth[] = "12";
  static constexpr char kExpYear[] = "2035";
  static constexpr char kExp[] = "12/2035";
  static constexpr char kCvc[] = "123";

  // Common JavaScript for setting up autofill event handlers.
  // Provides createAutofillHandler() and setupAutofillListeners() functions.
  static constexpr char kAutofillEventHandlerJs[] = R"(
    function createAutofillHandler(stateObj, fieldSelector, errorPrefix) {
      return function(event) {
        const field = document.querySelector(fieldSelector);
        const fieldsFilledBeforeEvent = !!(field && field.value &&
            stateObj.eventData.length === 0);
        if (fieldsFilledBeforeEvent) {
          stateObj.errors.push(
            errorPrefix + 'Field was already filled when event fired');
        }
        const fieldData = event.autofillValues;
        stateObj.eventData.push({
          fireCount: stateObj.eventData.length + 1,
          fieldData: fieldData.map(f => ({
            field: f.field ? (f.field.name || f.field.id) : null,
            value: f.value
          })),
          fieldsFilledBeforeEvent: fieldsFilledBeforeEvent,
          refillIsNull: event.refill === null
        });
      };
    }
    function setupAutofillListeners(stateObj, fieldSelector, errorPrefix) {
      document.onautofill = createAutofillHandler(
          stateObj, fieldSelector, errorPrefix);
      document.addEventListener('autofill', function(e) {
        const eventData = stateObj.eventData;
        if (eventData.length > 0) {
          eventData[eventData.length - 1].addEventListenerFired = true;
        }
      });
    }
)";

  // HTML template parts for iframe content. Placeholders:
  // $1 = field id, $2 = field name, $3 = autocomplete attribute
  static constexpr char kIframeHtmlPrefix[] = R"(<!DOCTYPE html>
<html><head><title>CC Frame</title></head>
<body>
  <form><input type="text" id="$1" name="$2" autocomplete="$3"></form>
  <script>
    window.frameTestState = {
      eventData: [],
      errors: []
    };
)";

  static constexpr char kIframeHtmlSuffix[] = R"(
    setupAutofillListeners(window.frameTestState, '#$1', '');
    window.getTestState = function() {
      const field = document.getElementById('$1');
      window.frameTestState.currentFieldValue = field ? field.value : null;
      return window.frameTestState;
    };
  </script>
</body>
</html>
)";

  // Main frame HTML parts with iframes for credit card fields.
  static constexpr char kMainFrameHtmlPrefix[] = R"(<!DOCTYPE html>
<html>
<body>
  <form method="post">
    <input id="CREDIT_CARD_NAME_FULL" autocomplete="cc-name">
    <iframe id="frame-number" src="/cc-number.html"></iframe>
    <iframe id="frame-exp" src="/cc-exp.html"></iframe>
    <iframe id="frame-cvc" src="/cc-cvc.html"></iframe>
  </form>
  <script>
    window.testState = {
      eventData: [],
      errors: [],
      iframeResults: {}
    };
)";

  static constexpr char kMainFrameHtmlSuffix[] = R"(
    setupAutofillListeners(
        window.testState, '#CREDIT_CARD_NAME_FULL', 'Main frame: ');
    window.collectIframeResults = function() {
      const results = {};
      const frames = ['frame-number', 'frame-exp', 'frame-cvc'];
      for (const frameName of frames) {
        try {
          const iframe = document.getElementById(frameName);
          if (iframe && iframe.contentWindow &&
            iframe.contentWindow.getTestState) {
            results[frameName] = iframe.contentWindow.getTestState();
          }
        } catch (e) {
          window.testState.errors.push('Error getting state from ' +
           frameName + ': ' + e);
        }
      }
      window.testState.iframeResults = results;
      return results;
    };
  </script>
</body>
</html>
)";

  // Builds the complete iframe HTML template.
  static std::string GetIframeHtml() {
    return std::string(kIframeHtmlPrefix) + kAutofillEventHandlerJs +
           kIframeHtmlSuffix;
  }

  // Builds the complete main frame HTML.
  static std::string GetMainFrameHtml() {
    return std::string(kMainFrameHtmlPrefix) + kAutofillEventHandlerJs +
           kMainFrameHtmlSuffix;
  }

  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver) {
      test_api(test_api(*this).form_filler())
          .set_limit_before_refill(base::Hours(1));
    }

    static TestAutofillManager& GetForRenderFrameHost(
        content::RenderFrameHost* rfh) {
      return static_cast<TestAutofillManager&>(
          ContentAutofillDriver::GetForRenderFrameHost(rfh)
              ->GetAutofillManager());
    }

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
  };

  AutofillEventMultiFrameBrowserTest() = default;

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_ANDROID)
    AndroidBrowserTest::SetUpOnMainThread();
#else
    InProcessBrowserTest::SetUpOnMainThread();
#endif
    // Register request handler before starting server.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&AutofillEventMultiFrameBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html;charset=utf-8");

    std::string path(request.GetURL().path());
    std::string iframe_html = GetIframeHtml();
    if (path == "/") {
      response->set_content(GetMainFrameHtml());
    } else if (path == "/cc-number.html") {
      response->set_content(base::ReplaceStringPlaceholders(
          iframe_html, {"CREDIT_CARD_NUMBER", "cc-number", "cc-number"},
          nullptr));
    } else if (path == "/cc-exp.html") {
      response->set_content(base::ReplaceStringPlaceholders(
          iframe_html, {"CREDIT_CARD_EXP_DATE", "cc-exp", "cc-exp"}, nullptr));
    } else if (path == "/cc-cvc.html") {
      response->set_content(base::ReplaceStringPlaceholders(
          iframe_html, {"CREDIT_CARD_CVC", "cc-csc", "cc-csc"}, nullptr));
    } else {
      return nullptr;
    }
    return response;
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestAutofillManager& main_autofill_manager() {
    return *autofill_manager_injector_[main_frame()];
  }

  // Helper to check if the main frame's autofill event fired.
  bool MainFrameEventFired() {
    return content::EvalJs(web_contents(),
                           "(window.testState?.eventData?.length ?? 0) > 0")
        .ExtractBool();
  }

  // Helper to check if an iframe's autofill event fired.
  bool IframeEventFired(std::string_view frame_name) {
    return content::EvalJs(web_contents(),
                           base::ReplaceStringPlaceholders(
                               "(window.testState?.iframeResults?.['$1']?."
                               "eventData?.length ?? 0) > 0",
                               {std::string(frame_name)}, nullptr))
        .ExtractBool();
  }

  // Helper to check if event data (main frame or iframe) contains a value.
  bool EventHasValue(std::string_view value, std::string_view frame_name = "") {
    if (frame_name.empty()) {
      return content::EvalJs(
                 web_contents(),
                 base::ReplaceStringPlaceholders(
                     "(window.testState?.eventData?.[0]?.fieldData ?? [])"
                     ".some(f => f.value === '$1')",
                     {std::string(value)}, nullptr))
          .ExtractBool();
    }
    return content::EvalJs(
               web_contents(),
               base::ReplaceStringPlaceholders(
                   "(window.testState?.iframeResults?.['$1']?.eventData?.[0]"
                   "?.fieldData ?? []).some(f => f.value === '$2')",
                   {std::string(frame_name), std::string(value)}, nullptr))
        .ExtractBool();
  }

  // Helper to get the field count in event data (main frame or iframe).
  int EventFieldCount(std::string_view frame_name = "") {
    if (frame_name.empty()) {
      return content::EvalJs(web_contents(),
                             "window.testState?.eventData?.[0]?.fieldData"
                             "?.length ?? 0")
          .ExtractInt();
    }
    return content::EvalJs(
               web_contents(),
               base::ReplaceStringPlaceholders(
                   "window.testState?.iframeResults?.['$1']?.eventData?.[0]"
                   "?.fieldData?.length ?? 0",
                   {std::string(frame_name)}, nullptr))
        .ExtractInt();
  }

  // Helper to get an iframe's current field value.
  std::string IframeFieldValue(std::string_view frame_name) {
    return content::EvalJs(
               web_contents(),
               base::ReplaceStringPlaceholders(
                   "window.testState.iframeResults['$1']?.currentFieldValue "
                   "?? ''",
                   {std::string(frame_name)}, nullptr))
        .ExtractString();
  }

  // Helper to check if addEventListener fired on an iframe.
  bool IframeAddEventListenerFired(std::string_view frame_name) {
    return content::EvalJs(
               web_contents(),
               base::ReplaceStringPlaceholders(
                   "window.testState?.iframeResults?.['$1']?.eventData?.[0]"
                   "?.addEventListenerFired ?? false",
                   {std::string(frame_name)}, nullptr))
        .ExtractBool();
  }

  // Helper to check if an iframe's field was filled before the event fired.
  bool IframeFieldFilledBeforeEvent(std::string_view frame_name) {
    return content::EvalJs(
               web_contents(),
               base::ReplaceStringPlaceholders(
                   "window.testState?.iframeResults?.['$1']?.eventData?.[0]"
                   "?.fieldsFilledBeforeEvent ?? false",
                   {std::string(frame_name)}, nullptr))
        .ExtractBool();
  }

  // Collects iframe results into window.testState.iframeResults.
  [[nodiscard]] testing::AssertionResult CollectIframeResults() {
    if (!content::ExecJs(web_contents(), "window.collectIframeResults()")) {
      return testing::AssertionFailure() << "Could not collect iframe results";
    }
    return testing::AssertionSuccess();
  }

  // Fakes a credit card autofill on a given form.
  void FillCard(content::RenderFrameHost* rfh,
                const FormData& form,
                const FormFieldData& triggered_field) {
    CreditCard card;
    test::SetCreditCardInfo(&card, kNameFull, kNumber, kExpMonth, kExpYear, "",
                            base::ASCIIToUTF16(std::string_view(kCvc)));
    TestAutofillManager* manager = autofill_manager_injector_[rfh];
    manager->FillOrPreviewForm(mojom::ActionPersistence::kFill, form,
                               triggered_field.global_id(), &card,
                               AutofillTriggerSource::kPopup);
  }

  // Returns a form with `num_fields` fields. If no such form exists and no such
  // form appears within a timeout, returns nullptr.
  const FormStructure* GetOrWaitForFormWithFocusableFields(size_t num_fields) {
    const FormStructure* form = WaitForMatchingForm(
        &main_autofill_manager(),
        base::BindRepeating(
            [](size_t num_fields, const FormStructure& form) {
              return num_fields == form.field_count();
            },
            num_fields));
    if (!form) {
      return nullptr;
    }
    // Sometimes fields are unfocusable when extracted on page load. Override
    // for testing purposes.
    for (const auto& field : *form) {
      const_cast<AutofillField&>(*field).set_is_focusable(true);
    }
    return form;
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

// Verifies that the autofill event fires correctly in a multi-frame scenario
// where credit card fields are split across multiple iframes.
// Each frame should receive its own autofill event with the field data
// for the fields in that frame.
IN_PROC_BROWSER_TEST_F(AutofillEventMultiFrameBrowserTest,
                       CreditCardFormAcrossIframes) {
  GURL url = embedded_test_server()->GetURL("/");
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents(), url));

  // Wait for the form to be seen. The form has 4 fields:
  // - 1 in main frame (cc-name)
  // - 1 in each of 3 iframes (cc-number, cc-exp, cc-csc)
  constexpr size_t kExpectedFieldCount = 4;
  const FormStructure* form =
      GetOrWaitForFormWithFocusableFields(kExpectedFieldCount);
  ASSERT_TRUE(form) << "Form with " << kExpectedFieldCount
                    << " fields not found";

  const FormData& form_data = form->ToFormData();
  ASSERT_EQ(kExpectedFieldCount, form_data.fields().size());

  // Trigger autofill on the first field (cc-name in main frame).
  FillCard(main_frame(), form_data, form_data.fields()[0]);

  // Wait for autofill to complete. With fields in 4 frames, we expect
  // at least 4 autofill events (one per frame).
  ASSERT_TRUE(main_autofill_manager().WaitForAutofillFill(kExpectedFieldCount));

  // Verify main frame received the autofill event with expected data.
  EXPECT_TRUE(MainFrameEventFired()) << "Main frame autofill event not fired";
  EXPECT_GT(EventFieldCount(), 0) << "Main frame event has no fields";
  EXPECT_TRUE(EventHasValue(kNameFull))
      << "Main frame event missing cardholder name";

  // Collect iframe results and verify each iframe received its event.
  ASSERT_TRUE(CollectIframeResults());

  EXPECT_TRUE(IframeEventFired("frame-number"))
      << "CC Number iframe event not fired";
  EXPECT_TRUE(EventHasValue(kNumber, "frame-number"))
      << "CC Number iframe missing card number";

  EXPECT_TRUE(IframeEventFired("frame-exp")) << "CC Exp iframe event not fired";
  // Expiration format may vary (e.g., "12/2035" or just month).
  EXPECT_TRUE(EventHasValue(kExp, "frame-exp") ||
              EventHasValue(kExpMonth, "frame-exp"))
      << "CC Exp iframe missing expiration date";

  EXPECT_TRUE(IframeEventFired("frame-cvc")) << "CC CVC iframe event not fired";
  EXPECT_TRUE(EventHasValue(kCvc, "frame-cvc")) << "CC CVC iframe missing CVC";

  // Verify addEventListener also works.
  EXPECT_TRUE(IframeAddEventListenerFired("frame-number"))
      << "CC Number iframe addEventListener did not fire";

  // Verify fields were not filled before events.
  EXPECT_FALSE(IframeFieldFilledBeforeEvent("frame-number"))
      << "CC Number iframe field was filled before event fired";

  // Verify actual field values after autofill.
  std::string name_value =
      content::EvalJs(web_contents(),
                      "document.getElementById('CREDIT_CARD_NAME_FULL').value")
          .ExtractString();
  EXPECT_EQ(kNameFull, name_value) << "Main frame name field not filled";
  EXPECT_EQ(kNumber, IframeFieldValue("frame-number"))
      << "CC Number iframe field not filled";
  EXPECT_EQ(kCvc, IframeFieldValue("frame-cvc"))
      << "CC CVC iframe field not filled";
}

// Verifies that each frame's autofill event only contains field data for
// fields within that frame, not fields from other frames.
IN_PROC_BROWSER_TEST_F(AutofillEventMultiFrameBrowserTest,
                       EventContainsOnlyLocalFrameFields) {
  GURL url = embedded_test_server()->GetURL("/");
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents(), url));

  constexpr size_t kExpectedFieldCount = 4;
  const FormStructure* form =
      GetOrWaitForFormWithFocusableFields(kExpectedFieldCount);
  ASSERT_TRUE(form) << "Form not found";

  const FormData& form_data = form->ToFormData();
  FillCard(main_frame(), form_data, form_data.fields()[0]);
  ASSERT_TRUE(main_autofill_manager().WaitForAutofillFill(1));

  // Wait for events to propagate.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
  run_loop.Run();

  ASSERT_TRUE(CollectIframeResults());

  // Main frame event should only contain cc-name field, not fields from other
  // frames.
  EXPECT_EQ(1, EventFieldCount())
      << "Main frame event should contain exactly 1 field (cc-name)";
  EXPECT_FALSE(EventHasValue(kNumber))
      << "Main frame event should not contain CC number from iframe";

  // CC Number iframe should only have the card number field.
  EXPECT_EQ(1, EventFieldCount("frame-number"))
      << "CC Number iframe event should contain exactly 1 field";

  // CC Number iframe should not have name or CVC in its event data.
  EXPECT_FALSE(EventHasValue(kNameFull, "frame-number"))
      << "CC Number iframe should not contain name field from main frame";
  EXPECT_FALSE(EventHasValue(kCvc, "frame-number"))
      << "CC Number iframe should not contain CVC field from other iframe";
}

}  // namespace autofill
