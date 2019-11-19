// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
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
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::ASCIIToUTF16;
using content::URLLoaderInterceptor;

namespace autofill {

namespace {

static const char kTestShippingFormString[] =
    "<form action=\"http://www.example.com/\" method=\"POST\">"
    "<label for=\"firstname\">First name:</label>"
    " <input type=\"text\" id=\"firstname\"><br>"
    "<label for=\"lastname\">Last name:</label>"
    " <input type=\"text\" id=\"lastname\"><br>"
    "<label for=\"address1\">Address line 1:</label>"
    " <input type=\"text\" id=\"address1\"><br>"
    "<label for=\"address2\">Address line 2:</label>"
    " <input type=\"text\" id=\"address2\"><br>"
    "<label for=\"city\">City:</label>"
    " <input type=\"text\" id=\"city\"><br>"
    "<label for=\"state\">State:</label>"
    " <select id=\"state\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">California</option>"
    " <option value=\"TX\">Texas</option>"
    " </select><br>"
    "<label for=\"zip\">ZIP code:</label>"
    " <input type=\"text\" id=\"zip\"><br>"
    "<label for=\"country\">Country:</label>"
    " <select id=\"country\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">Canada</option>"
    " <option value=\"US\">United States</option>"
    " </select><br>"
    "<label for=\"phone\">Phone number:</label>"
    " <input type=\"text\" id=\"phone\"><br>"
    "</form>";

static const char kTestBillingFormString[] =
    "<form action=\"http://www.example.com/\" method=\"POST\">"
    "<label for=\"firstname_billing\">First name:</label>"
    " <input type=\"text\" id=\"firstname_billing\"><br>"
    "<label for=\"lastname_billing\">Last name:</label>"
    " <input type=\"text\" id=\"lastname_billing\"><br>"
    "<label for=\"address1_billing\">Address line 1:</label>"
    " <input type=\"text\" id=\"address1_billing\"><br>"
    "<label for=\"address2_billing\">Address line 2:</label>"
    " <input type=\"text\" id=\"address2_billing\"><br>"
    "<label for=\"city_billing\">City:</label>"
    " <input type=\"text\" id=\"city_billing\"><br>"
    "<label for=\"state_billing\">State:</label>"
    " <select id=\"state_billing\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">California</option>"
    " <option value=\"TX\">Texas</option>"
    " </select><br>"
    "<label for=\"zip_billing\">ZIP code:</label>"
    " <input type=\"text\" id=\"zip_billing\"><br>"
    "<label for=\"country_billing\">Country:</label>"
    " <select id=\"country_billing\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">Canada</option>"
    " <option value=\"US\">United States</option>"
    " </select><br>"
    "<label for=\"phone_billing\">Phone number:</label>"
    " <input type=\"text\" id=\"phone_billing\"><br>"
    "</form>";

// TODO(crbug.com/609861): Remove the autocomplete attribute from the textarea
// field when the bug is fixed.
static const char kTestEventFormString[] =
    "<script type=\"text/javascript\">"
    "var inputfocus = false;"
    "var inputkeydown = false;"
    "var inputinput = false;"
    "var inputchange = false;"
    "var inputkeyup = false;"
    "var inputblur = false;"
    "var textfocus = false;"
    "var textkeydown = false;"
    "var textinput= false;"
    "var textchange = false;"
    "var textkeyup = false;"
    "var textblur = false;"
    "var selectfocus = false;"
    "var selectinput = false;"
    "var selectchange = false;"
    "var selectblur = false;"
    "</script>"
    "<form action=\"http://www.example.com/\" method=\"POST\">"
    "<label for=\"firstname\">First name:</label>"
    " <input type=\"text\" id=\"firstname\"><br>"
    "<label for=\"lastname\">Last name:</label>"
    " <input type=\"text\" id=\"lastname\""
    " onfocus=\"inputfocus = true\" onkeydown=\"inputkeydown = true\""
    " oninput=\"inputinput = true\" onchange=\"inputchange = true\""
    " onkeyup=\"inputkeyup = true\" onblur=\"inputblur = true\" ><br>"
    "<label for=\"address1\">Address line 1:</label>"
    " <input type=\"text\" id=\"address1\"><br>"
    "<label for=\"address2\">Address line 2:</label>"
    " <input type=\"text\" id=\"address2\"><br>"
    "<label for=\"city\">City:</label>"
    " <textarea rows=\"4\" cols=\"50\" id=\"city\" name=\"city\""
    " autocomplete=\"address-level2\" onfocus=\"textfocus = true\""
    " onkeydown=\"textkeydown = true\" oninput=\"textinput = true\""
    " onchange=\"textchange = true\" onkeyup=\"textkeyup = true\""
    " onblur=\"textblur = true\"></textarea><br>"
    "<label for=\"state\">State:</label>"
    " <select id=\"state\""
    " onfocus=\"selectfocus = true\" oninput=\"selectinput = true\""
    " onchange=\"selectchange = true\" onblur=\"selectblur = true\" >"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">California</option>"
    " <option value=\"NY\">New York</option>"
    " <option value=\"TX\">Texas</option>"
    " </select><br>"
    "<label for=\"zip\">ZIP code:</label>"
    " <input type=\"text\" id=\"zip\"><br>"
    "<label for=\"country\">Country:</label>"
    " <select id=\"country\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">Canada</option>"
    " <option value=\"US\">United States</option>"
    " </select><br>"
    "<label for=\"phone\">Phone number:</label>"
    " <input type=\"text\" id=\"phone\"><br>"
    "</form>";

static const char kTestShippingFormWithCompanyString[] =
    "<form action=\"http://www.example.com/\" method=\"POST\">"
    "<label for=\"firstname\">First name:</label>"
    " <input type=\"text\" id=\"firstname\"><br>"
    "<label for=\"lastname\">Last name:</label>"
    " <input type=\"text\" id=\"lastname\"><br>"
    "<label for=\"address1\">Address line 1:</label>"
    " <input type=\"text\" id=\"address1\"><br>"
    "<label for=\"address2\">Address line 2:</label>"
    " <input type=\"text\" id=\"address2\"><br>"
    "<label for=\"city\">City:</label>"
    " <input type=\"text\" id=\"city\"><br>"
    "<label for=\"state\">State:</label>"
    " <select id=\"state\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">California</option>"
    " <option value=\"TX\">Texas</option>"
    " </select><br>"
    "<label for=\"zip\">ZIP code:</label>"
    " <input type=\"text\" id=\"zip\"><br>"
    "<label for=\"country\">Country:</label>"
    " <select id=\"country\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">Canada</option>"
    " <option value=\"US\">United States</option>"
    " </select><br>"
    "<label for=\"phone\">Phone number:</label>"
    " <input type=\"text\" id=\"phone\"><br>"
    "<label for=\"company\">First company:</label>"
    " <input type=\"text\" id=\"company\"><br>"
    "</form>";
// Searches all frames of |web_contents| and returns one called |name|. If
// there are none, returns null, if there are more, returns an arbitrary one.
content::RenderFrameHost* RenderFrameHostForName(
    content::WebContents* web_contents,
    const std::string& name) {
  return content::FrameMatchingPredicate(
      web_contents, base::BindRepeating(&content::FrameMatchesName, name));
}

}  // namespace

// AutofillInteractiveTestBase ------------------------------------------------

// Test fixtures derive from this class and indicate via constructor parameter
// if feature kAutofillExpandedPopupViews is enabled. This class hierarchy
// allows test fixtures to have distinct list of test parameters.
//
// TODO(crbug.com/832707): Parametrize this class to ensure that all tests in
//                         this run with all possible valid combinations of
//                         features and field trials.
class AutofillInteractiveTestBase : public AutofillUiTest {
 protected:
  AutofillInteractiveTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~AutofillInteractiveTestBase() override {}

  // InProcessBrowserTest:
  void SetUp() override {
    LOG(ERROR) << "crbug/967588: AutofillInteractiveTestBase::SetUp() entered";
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    LOG(ERROR) << "crbug/967588: embedded_test_server InitializeAndListen";
    InProcessBrowserTest::SetUp();
    LOG(ERROR) << "crbug/967588: AutofillInteractiveTestBase::SetUp() exited";
  }

  void SetUpOnMainThread() override {
    LOG(ERROR) << "crbug/967588: "
                  "AutofillInteractiveTestBase::SetUpOnMainThread() entered";
    AutofillUiTest::SetUpOnMainThread();

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &AutofillInteractiveTestBase::HandleTestURL, base::Unretained(this)));
    ASSERT_TRUE(https_server_.InitializeAndListen());
    https_server_.StartAcceptingConnections();
    LOG(ERROR) << "crbug/967588: https_server started accepting connections";

    controllable_http_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/mock_translate_script.js",
            true /*relative_url_is_prefix*/);

    // Ensure that |embedded_test_server()| serves both domains used below.
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AutofillInteractiveTestBase::HandleTestURL, base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
    LOG(ERROR)
        << "crbug/967588: embedded_test_server started accepting connections";

    // By default, all SSL cert checks are valid. Can be overriden in tests if
    // needed.
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    LOG(ERROR) << "crbug/967588: "
                  "AutofillInteractiveTestBase::SetUpOnMainThread() exited";
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillUiTest::SetUpCommandLine(command_line);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    AutofillUiTest::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
    AutofillUiTest::TearDownInProcessBrowserTestFixture();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleTestURL(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != kTestUrlPath)
      return nullptr;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html;charset=utf-8");
    response->set_content(test_url_content_);
    return std::move(response);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderViewHost* GetRenderViewHost() {
    return GetWebContents()->GetRenderViewHost();
  }

  void CreateTestProfile() {
    AutofillProfile profile;
    test::SetProfileInfo(&profile, "Milton", "C.", "Waddams",
                         "red.swingline@initech.com", "Initech",
                         "4120 Freidrich Lane", "Basement", "Austin", "Texas",
                         "78744", "US", "15125551234");
    profile.set_use_count(9999999);  // We want this to be the first profile.
    AddTestProfile(browser(), profile);
  }

  void CreateSecondTestProfile() {
    AutofillProfile profile;
    test::SetProfileInfo(&profile, "Alice", "M.", "Wonderland",
                         "alice@wonderland.com", "Magic", "333 Cat Queen St.",
                         "Rooftop", "Liliput", "CA", "10003", "US",
                         "15166900292");
    AddTestProfile(browser(), profile);
  }

  void CreateTestCreditCart() {
    CreditCard card;
    test::SetCreditCardInfo(&card, "Milton Waddams", "4111111111111111", "09",
                            "2999", "");
    AddTestCreditCard(browser(), card);
  }

  // Populates a webpage form using autofill data and keypress events.
  // This function focuses the specified input field in the form, and then
  // sends keypress events to the tab to cause the form to be populated.
  void PopulateForm(const std::string& field_id) {
    std::string js("document.getElementById('" + field_id + "').focus();");
    ASSERT_TRUE(content::ExecuteScript(GetWebContents(), js));

    ShowDropdownAndSelectFirstSuggestionUsingArrowDown();
    SendKeyToPopupAndWait(ui::DomKey::ENTER,
                          {ObservedUiEvents::kFormDataFilled});
  }

  void ExpectFieldValue(const std::string& field_name,
                        const std::string& expected_value) {
    std::string value;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        GetWebContents(),
        "window.domAutomationController.send("
        "    document.getElementById('" + field_name + "').value);",
        &value));
    EXPECT_EQ(expected_value, value) << "for field " << field_name;
  }

  void AssertFieldValue(const std::string& field_name,
                        const std::string& expected_value) {
    std::string value;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        GetWebContents(),
        "window.domAutomationController.send("
        "    document.getElementById('" +
            field_name + "').value);",
        &value));
    ASSERT_EQ(expected_value, value) << "for field " << field_name;
  }

  void GetFieldBackgroundColor(const std::string& field_name,
                               std::string* color) {
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        GetWebContents(),
        "window.domAutomationController.send("
        "    document.defaultView.getComputedStyle(document.getElementById('" +
        field_name + "')).backgroundColor);",
        color));
  }

  void SimulateURLFetch() {
    std::string script =
        " var google = {};"
        "google.translate = (function() {"
        "  return {"
        "    TranslateService: function() {"
        "      return {"
        "        isAvailable : function() {"
        "          return true;"
        "        },"
        "        restore : function() {"
        "          return;"
        "        },"
        "        getDetectedLanguage : function() {"
        "          return \"ja\";"
        "        },"
        "        translatePage : function(originalLang, targetLang,"
        "                                 onTranslateProgress) {"
        "          document.getElementsByTagName(\"body\")[0].innerHTML = '" +
        std::string(kTestShippingFormString) +
        "              ';"
        "          onTranslateProgress(100, true, false);"
        "        }"
        "      };"
        "    }"
        "  };"
        "})();"
        "cr.googleTranslate.onTranslateElementLoad();";

    controllable_http_response_->WaitForRequest();
    controllable_http_response_->Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/javascript\r\n"
        "\r\n");
    controllable_http_response_->Send(script);
    controllable_http_response_->Done();
  }

  void FocusFieldByName(const std::string& name) {
    bool result = false;
    std::string script = base::StringPrintf(
        R"( function onFocusHandler(e) {
              e.target.removeEventListener(e.type, arguments.callee);
              domAutomationController.send(true);
            }
            if (document.readyState === 'complete') {
              var target = document.getElementById('%s');
              target.addEventListener('focus', onFocusHandler);
              target.focus();
            } else {
              domAutomationController.send(false);
            })",
        name.c_str());
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(GetWebContents(), script,
                                                     &result));
    ASSERT_TRUE(result);
  }

  void FocusFirstNameField() { FocusFieldByName("firstname"); }

  // Simulates a click on the middle of the DOM element with the given |id|.
  void ClickElementWithId(const std::string& id) {
    int x;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        GetWebContents(),
        "var bounds = document.getElementById('" + id +
            "').getBoundingClientRect();"
            "domAutomationController.send("
            "    Math.floor(bounds.left + bounds.width / 2));",
        &x));
    int y;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        GetWebContents(),
        "var bounds = document.getElementById('" + id +
            "').getBoundingClientRect();"
            "domAutomationController.send("
            "    Math.floor(bounds.top + bounds.height / 2));",
        &y));
    content::SimulateMouseClickAt(GetWebContents(), 0,
                                  blink::WebMouseEvent::Button::kLeft,
                                  gfx::Point(x, y));
  }

  void ClickFirstNameField() {
    ASSERT_NO_FATAL_FAILURE(ClickElementWithId("firstname"));
  }

  // Make a pointless round trip to the renderer, giving the popup a chance to
  // show if it's going to. If it does show, an assert in
  // AutofillManagerTestDelegateImpl will trigger.
  void MakeSurePopupDoesntAppear() {
    int unused;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        GetWebContents(), "domAutomationController.send(42)", &unused));
  }

  void ExpectFilledTestForm() {
    ExpectFieldValue("firstname", "Milton");
    ExpectFieldValue("lastname", "Waddams");
    ExpectFieldValue("address1", "4120 Freidrich Lane");
    ExpectFieldValue("address2", "Basement");
    ExpectFieldValue("city", "Austin");
    ExpectFieldValue("state", "TX");
    ExpectFieldValue("zip", "78744");
    ExpectFieldValue("country", "US");
    ExpectFieldValue("phone", "15125551234");
    LOG(ERROR) << "crbug/967588: Verified form was filled as expected";
  }

  void ExpectClearedForm() {
    ExpectFieldValue("firstname", "");
    ExpectFieldValue("lastname", "");
    ExpectFieldValue("address1", "");
    ExpectFieldValue("address2", "");
    ExpectFieldValue("city", "");
    ExpectFieldValue("state", "");
    ExpectFieldValue("zip", "");
    ExpectFieldValue("country", "");
    ExpectFieldValue("phone", "");
  }

  void FillElementWithValue(const std::string& element_name,
                            const std::string& value) {
    for (base::char16 character : value) {
      ui::DomKey dom_key = ui::DomKey::FromCharacter(character);
      const ui::PrintableCodeEntry* code_entry = std::find_if(
          std::begin(ui::kPrintableCodeMap), std::end(ui::kPrintableCodeMap),
          [character](const ui::PrintableCodeEntry& entry) {
            return entry.character[0] == character ||
                   entry.character[1] == character;
          });
      ASSERT_TRUE(code_entry != std::end(ui::kPrintableCodeMap));
      bool shift = code_entry->character[1] == character;
      ui::DomCode dom_code = code_entry->dom_code;
      content::SimulateKeyPress(GetWebContents(), dom_key, dom_code,
                                ui::DomCodeToUsLayoutKeyboardCode(dom_code),
                                false, shift, false, false);
    }
    AssertFieldValue(element_name, value);
  }

  void DeleteElementValue(const std::string& element_name) {
    ASSERT_TRUE(content::ExecuteScript(
        GetWebContents(),
        "document.getElementById('" + element_name + "').value = '';"));
    AssertFieldValue(element_name, "");
  }

  void TryBasicFormFill() {
    FocusFirstNameField();
    LOG(ERROR) << "crbug/967588: Focussed first name field";

    // Start filling the first name field with "M" and wait for the popup to be
    // shown.
    SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                         ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});

    LOG(ERROR) << "crbug/967588: Sent 'M' and saw suggestion";

    // Press the down arrow to select the suggestion and preview the autofilled
    // form.
    SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                          {ObservedUiEvents::kPreviewFormData});

    LOG(ERROR) << "crbug/967588: Sent '<down arrow>' and saw preview";

    // The previewed values should not be accessible to JavaScript.
    ExpectFieldValue("firstname", "M");
    ExpectFieldValue("lastname", std::string());
    ExpectFieldValue("address1", std::string());
    ExpectFieldValue("address2", std::string());
    ExpectFieldValue("city", std::string());
    ExpectFieldValue("state", std::string());
    ExpectFieldValue("zip", std::string());
    ExpectFieldValue("country", std::string());
    ExpectFieldValue("phone", std::string());
    // TODO(isherman): It would be nice to test that the previewed values are
    // displayed: http://crbug.com/57220

    LOG(ERROR)
        << "crbug/967588: Verified field contents remain unfilled for preview";

    // Press Enter to accept the autofill suggestions.
    SendKeyToPopupAndWait(ui::DomKey::ENTER,
                          {ObservedUiEvents::kFormDataFilled});

    LOG(ERROR) << "crbug/967588: Form was filled after pressing enter";

    // The form should be filled.
    ExpectFilledTestForm();
  }

  void ShowDropdownAndSelectFirstSuggestionUsingArrowDown(
      content::RenderWidgetHost* widget = nullptr) {
    if (ShouldAutoselectFirstSuggestionOnArrowDown()) {
      SendKeyToPageAndWait(ui::DomKey::ARROW_DOWN,
                           {ObservedUiEvents::kSuggestionShown,
                            ObservedUiEvents::kPreviewFormData});
    } else {
      SendKeyToPageAndWait(ui::DomKey::ARROW_DOWN,
                           {ObservedUiEvents::kSuggestionShown});
      SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                            {ObservedUiEvents::kPreviewFormData}, widget);
    }
  }

  void TryClearForm() {
    ShowDropdownAndSelectFirstSuggestionUsingArrowDown();
    SendKeyToDataListPopup(ui::DomKey::ARROW_DOWN);  // clear
    SendKeyToDataListPopup(ui::DomKey::ENTER);

    ExpectClearedForm();
  }

  void TriggerFormFill(const std::string& field_name) {
    FocusFieldByName(field_name);

    // Start filling the first name field with "M" and wait for the popup to be
    // shown.
    SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                         ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});

    // Press the down arrow to select the suggestion and preview the autofilled
    // form.
    SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                          {ObservedUiEvents::kPreviewFormData});

    // Press Enter to accept the autofill suggestions.
    SendKeyToPopupAndWait(ui::DomKey::ENTER,
                          {ObservedUiEvents::kFormDataFilled});
  }

  // Note: suggestion_position is 1-based, so 1 corresponds to the first
  // position, 2 to second position, and so on.
  void AcceptSuggestionUsingArrowDown(
      int suggestion_position = 1,
      content::RenderWidgetHost* widget = nullptr) {
    // Show the dropdown and select the first suggestion using arrow down.
    ShowDropdownAndSelectFirstSuggestionUsingArrowDown(widget);

    // If not selecting the first suggestion, press the down arrow
    // |suggestion_position - 1| times to select the suggestion requested and
    // preview the autofilled form.
    for (int i = 1; i < suggestion_position; ++i) {
      SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                            {ObservedUiEvents::kPreviewFormData}, widget);
    }

    // Press Enter to accept the autofill suggestions.
    SendKeyToPopupAndWait(ui::DomKey::ENTER,
                          {ObservedUiEvents::kFormDataFilled}, widget);
  }

  GURL GetTestUrl() const { return https_server_.GetURL(kTestUrlPath); }

  void SetTestUrlResponse(std::string content) {
    test_url_content_ = std::move(content);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  static const char kTestUrlPath[];

 private:
  net::EmbeddedTestServer https_server_;

  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;

  // KeyPressEventCallback that serves as a sink to ensure that every key press
  // event the tests create and have the WebContents forward is handled by some
  // key press event callback. It is necessary to have this sinkbecause if no
  // key press event callback handles the event (at least on Mac), a DCHECK
  // ends up going off that the |event| doesn't have an |os_event| associated
  // with it.
  content::RenderWidgetHost::KeyPressEventCallback key_press_event_sink_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_http_response_;

  // The response to return for queries to |kTestUrlPath|
  std::string test_url_content_;

  DISALLOW_COPY_AND_ASSIGN(AutofillInteractiveTestBase);
};

const char AutofillInteractiveTestBase::kTestUrlPath[] =
    "/internal/test_url_path";

// AutofillInteractiveTest ----------------------------------------------------

class AutofillInteractiveTest : public AutofillInteractiveTestBase {
 protected:
  AutofillInteractiveTest() = default;
  ~AutofillInteractiveTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillInteractiveTestBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        translate::switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
  }
};

class AutofillInteractiveTestWithHistogramTester
    : public AutofillInteractiveTest {
 public:
  void SetUp() override {
    // Only allow requests to be loaded that are necessary for the test. This
    // allows a histogram to test properties of some specific requests.
    std::vector<std::string> allowlist = {
        "/internal/test_url_path", "https://clients1.google.com/tbproxy"};
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) {
              for (const auto& s : allowlist) {
                const bool is_match =
                    params->url_request.url.spec().find(s) != std::string::npos;
                if (is_match)
                  return false;  // Do not intercept.
              }
              return true;  // Intercept
            }));
    AutofillInteractiveTest::SetUp();
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    AutofillInteractiveTest::TearDownOnMainThread();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

// Test that basic form fill is working.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestWithHistogramTester,
                       BasicFormFill) {
  LOG(ERROR) << "crbug/967588: In case of flakes, report log statements to "
                "crbug.com/967588";
  CreateTestProfile();
  LOG(ERROR) << "crbug/967588: Test profile created";

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));
  LOG(ERROR) << "crbug/967588: Loaded test page";

  // Invoke Autofill.
  TryBasicFormFill();
  LOG(ERROR) << "crbug/967588: Basic form filling completed";

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // Assert that the network isolation key is populated for 2 requests:
  // - Navigation: /internal/test_url_path
  // - Autofill query: https://clients1.google.com/tbproxy/af/query?...
  histogram_tester().ExpectBucketCount("HttpCache.NetworkIsolationKeyPresent2",
                                       2 /*kPresent*/, 2 /*count*/);
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, BasicClear) {
  CreateTestProfile();

  SetTestUrlResponse(kTestShippingFormString);

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  TryBasicFormFill();

  TryClearForm();
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ClearTwoSection) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(
      base::StrCat({kTestShippingFormString, kTestBillingFormString}));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Fill first section.
  TryBasicFormFill();

  // Fill second section.
  FocusFieldByName("firstname_billing");
  AcceptSuggestionUsingArrowDown();

  // Clear second section.
  ShowDropdownAndSelectFirstSuggestionUsingArrowDown();
  SendKeyToDataListPopup(ui::DomKey::ARROW_DOWN);  // clear
  SendKeyToDataListPopup(ui::DomKey::ENTER);

  ExpectFieldValue("firstname_billing", "");
  ExpectFieldValue("lastname_billing", "");
  ExpectFieldValue("address1_billing", "");
  ExpectFieldValue("address2_billing", "");
  ExpectFieldValue("city_billing", "");
  ExpectFieldValue("state_billing", "");
  ExpectFieldValue("zip_billing", "");
  ExpectFieldValue("country_billing", "");
  ExpectFieldValue("phone_billing", "");

  // First section should still be filled.
  ExpectFilledTestForm();
}

// Test that autofill doesn't refill a text field initially modified by the
// user.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ModifyTextFieldAndFill) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Modify a field.
  FocusFieldByName("city");
  FillElementWithValue("city", "Montreal");

  // Fill
  FocusFirstNameField();
  AcceptSuggestionUsingArrowDown();

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("lastname", "Waddams");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("address2", "Basement");
  ExpectFieldValue("city", "Montreal");  // Modified by the user.
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("zip", "78744");
  ExpectFieldValue("country", "US");
  ExpectFieldValue("phone", "15125551234");
}

// Test that autofill doesn't refill a select field initially modified by the
// user.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ModifySelectFieldAndFill) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Modify a field.
  FocusFieldByName("state");
  FillElementWithValue("state", "CA");

  // Fill
  FocusFirstNameField();
  AcceptSuggestionUsingArrowDown();

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("lastname", "Waddams");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("address2", "Basement");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("state", "CA");  // Modified by the user.
  ExpectFieldValue("zip", "78744");
  ExpectFieldValue("country", "US");
  ExpectFieldValue("phone", "15125551234");
}

// Test that autofill works when the website prefills the form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, PrefillFormAndFill) {
  const char kPrefillScript[] =
      "<script>"
      "document.getElementById('firstname').value = 'Seb';"
      "document.getElementById('lastname').value = 'Bell';"
      "document.getElementById('address1').value = '3243 Notre-Dame Ouest';"
      "document.getElementById('address2').value = 'apt 843';"
      "document.getElementById('city').value = 'Montreal';"
      "document.getElementById('zip').value = 'H5D 4D3';"
      "document.getElementById('phone').value = '15142223344';"
      "</script>";

  // Load the test page.
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kPrefillScript}));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  CreateTestProfile();

  // We need to delete the prefilled value and then trigger the autofill.
  FocusFirstNameField();
  DeleteElementValue("firstname");

  AcceptSuggestionUsingArrowDown();
  ExpectFilledTestForm();
}

// Test that autofill doesn't refill a field modified by the user.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       FillChangeSecondFieldRefillAndClearFirstFill) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  TryBasicFormFill();

  // Change the last name.
  FocusFieldByName("lastname");
  SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                       {ObservedUiEvents::kSuggestionShown});
  SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                       {ObservedUiEvents::kSuggestionShown});

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("lastname", "Wadda");  // Modified by the user.
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("address2", "Basement");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("zip", "78744");
  ExpectFieldValue("country", "US");
  ExpectFieldValue("phone", "15125551234");

  // Fill again by focusing on the first field.
  FocusFirstNameField();
  AcceptSuggestionUsingArrowDown();

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("lastname",
                   "Wadda");  // Modified by the user, should not be autofilled.
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("address2", "Basement");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("zip", "78744");
  ExpectFieldValue("country", "US");
  ExpectFieldValue("phone", "15125551234");

  // Clear everything except last name by selecting 'clear' on the first field.
  ShowDropdownAndSelectFirstSuggestionUsingArrowDown();
  SendKeyToDataListPopup(ui::DomKey::ARROW_DOWN);  // clear
  SendKeyToDataListPopup(ui::DomKey::ENTER);

  ExpectFieldValue("firstname", "");
  ExpectFieldValue("lastname",
                   "Wadda");  // Modified by the user, should not be autofilled.
  ExpectFieldValue("address1", "");
  ExpectFieldValue("address2", "");
  ExpectFieldValue("city", "");
  ExpectFieldValue("state", "");
  ExpectFieldValue("zip", "");
  ExpectFieldValue("country", "");
  ExpectFieldValue("phone", "");
}

// Test that multiple autofillings work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       FillChangeSecondFieldRefillAndClearSecondField) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  TryBasicFormFill();

  // Change the last name.
  FocusFieldByName("lastname");
  SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                       {ObservedUiEvents::kSuggestionShown});
  SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                       {ObservedUiEvents::kSuggestionShown});

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("lastname", "Wadda");  // Modified by the user.
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("address2", "Basement");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("zip", "78744");
  ExpectFieldValue("country", "US");
  ExpectFieldValue("phone", "15125551234");

  // Autofill the last name.
  // Note: the dropdown is already visible at this point, no need to send an
  // ARROW_DOWN to the page to show it.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  ExpectFilledTestForm();

  TryClearForm();
}

// Test that multiple autofillings work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       FillChangeSecondFieldRefillSecondFieldClearFirst) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));
  TryBasicFormFill();

  // Change the last name.
  FocusFieldByName("lastname");
  SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                       {ObservedUiEvents::kSuggestionShown});
  SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                       {ObservedUiEvents::kSuggestionShown});

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("lastname", "Wadda");  // Modified by the user.
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("address2", "Basement");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("zip", "78744");
  ExpectFieldValue("country", "US");
  ExpectFieldValue("phone", "15125551234");

  // Autofill the last name.
  // Note: the dropdown is already visible at this point, no need to send an
  // ARROW_DOWN to the page to show it.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  ExpectFilledTestForm();

  // Clear everything by selecting 'clear' on the first field.
  FocusFirstNameField();
  TryClearForm();
}

// Test that multiple autofillings work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       FillThenFillSomeWithAnotherProfileThenClear) {
  CreateTestProfile();
  CreateSecondTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  TryBasicFormFill();

  // Delete some fields.
  FocusFieldByName("city");
  DeleteElementValue("city");
  FocusFieldByName("address1");
  DeleteElementValue("address1");

  AcceptSuggestionUsingArrowDown(/*suggestion_position=*/2);

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("lastname", "Waddams");
  ExpectFieldValue("address1", "333 Cat Queen St.");  // second profile
  ExpectFieldValue("address2", "Basement");
  ExpectFieldValue("city", "Liliput");  // second profile
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("zip", "78744");
  ExpectFieldValue("country", "US");
  ExpectFieldValue("phone", "15125551234");

  // Clear everything by selecting 'clear' on the first field.
  FocusFirstNameField();
  TryClearForm();
}

// Test that form filling can be initiated by pressing the down arrow.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillViaDownArrow) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Focus a fillable field.
  FocusFirstNameField();

  AcceptSuggestionUsingArrowDown();

  // The form should be filled.
  ExpectFilledTestForm();
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillSelectViaTab) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Focus a fillable field.
  FocusFirstNameField();

  AcceptSuggestionUsingArrowDown();

  // The form should be filled.
  ExpectFilledTestForm();
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillViaClick) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Focus a fillable field.
  ASSERT_NO_FATAL_FAILURE(FocusFirstNameField());

  // Now click it.
  test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionShown});
  ASSERT_NO_FATAL_FAILURE(ClickFirstNameField());
  EXPECT_TRUE(test_delegate()->Wait());

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  // The form should be filled.
  ExpectFilledTestForm();
}

// Test params:
//  - bool popup_views_enabled: whether feature AutofillExpandedPopupViews
//        is enabled for testing.
//  - bool company_name_enabled_: whether feature AutofillEnableCompanyName
//        is enabled for testing.
class AutofillCompanyInteractiveTest
    : public AutofillInteractiveTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  AutofillCompanyInteractiveTest()
      : AutofillInteractiveTestBase(), company_name_enabled_(GetParam()) {}
  ~AutofillCompanyInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillEnableCompanyName, company_name_enabled_);
    AutofillInteractiveTestBase::SetUp();
  }

  const bool company_name_enabled_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Makes sure that the first click does or does not activate the autofill popup
// on the initial click within a fillable field.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, Click) {
  // Make sure autofill data exists.
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // This click should activate the autofill popup.
  test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionShown});
  ASSERT_NO_FATAL_FAILURE(ClickFirstNameField());
  EXPECT_TRUE(test_delegate()->Wait());

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  // The form should be filled.
  ExpectFilledTestForm();
}

// Makes sure that clicking outside the focused field doesn't activate
// the popup.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DontAutofillForOutsideClick) {
  static const char kDisabledButton[] =
      "<button disabled id='disabled-button'>Cant click this</button>";

  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kDisabledButton}));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_NO_FATAL_FAILURE(FocusFirstNameField());

  // Clicking a disabled button will generate a mouse event but focus doesn't
  // change. This tests that autofill can handle a mouse event outside a focused
  // input *without* showing the popup.
  ASSERT_NO_FATAL_FAILURE(ClickElementWithId("disabled-button"));
  ASSERT_NO_FATAL_FAILURE(MakeSurePopupDoesntAppear());

  test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionShown});
  ASSERT_NO_FATAL_FAILURE(ClickFirstNameField());
  EXPECT_TRUE(test_delegate()->Wait());
}

// Test that a field is still autofillable after the previously autofilled
// value is deleted.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnDeleteValueAfterAutofill) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke and accept the Autofill popup and verify the form was filled.
  FocusFirstNameField();
  SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                       ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});
  ExpectFilledTestForm();

  // Delete the value of a filled field.
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(), "document.getElementById('firstname').value = '';"));
  ExpectFieldValue("firstname", "");

  // Invoke and accept the Autofill popup and verify the field was filled.
  SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                       ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});
  ExpectFieldValue("firstname", "Milton");
}

// Test that an input field is not rendered with the yellow autofilled
// background color when choosing an option from the datalist suggestion list.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnSelectOptionFromDatalist) {
  static const char kTestForm[] =
      "<form action=\"http://www.example.com/\" method=\"POST\">"
      "  <input list=\"dl\" type=\"search\" id=\"firstname\"><br>"
      "  <datalist id=\"dl\">"
      "  <option value=\"Adam\"></option>"
      "  <option value=\"Bob\"></option>"
      "  <option value=\"Carl\"></option>"
      "  </datalist>"
      "</form>";

  // Load the test page.
  SetTestUrlResponse(kTestForm);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  std::string orginalcolor;
  GetFieldBackgroundColor("firstname", &orginalcolor);

  FocusFirstNameField();
  SendKeyToPageAndWait(ui::DomKey::ARROW_DOWN,
                       {ObservedUiEvents::kSuggestionShown});
  SendKeyToDataListPopup(ui::DomKey::ARROW_DOWN);
  SendKeyToDataListPopup(ui::DomKey::ENTER);
  // Pressing the down arrow preselects the first item. Pressing it again
  // selects the second item.
  ExpectFieldValue("firstname", "Bob");
  std::string color;
  GetFieldBackgroundColor("firstname", &color);
  EXPECT_EQ(color, orginalcolor);
}

// Test that an <input> field with a <datalist> has a working drop down even if
// it was dynamically changed to <input type="password"> temporarily. This is a
// regression test for crbug.com/918351.
IN_PROC_BROWSER_TEST_F(
    AutofillInteractiveTest,
    OnSelectOptionFromDatalistTurningToPasswordFieldAndBack) {
  static const char kTestForm[] =
      "<form action=\"http://www.example.com/\" method=\"POST\">"
      "  <input list=\"dl\" type=\"search\" id=\"firstname\"><br>"
      "  <datalist id=\"dl\">"
      "  <option value=\"Adam\"></option>"
      "  <option value=\"Bob\"></option>"
      "  <option value=\"Carl\"></option>"
      "  </datalist>"
      "</form>";

  // Load the test page.
  SetTestUrlResponse(kTestForm);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(),
      "document.getElementById('firstname').type = 'password';"));
  // At this point, the IsPasswordFieldForAutofill() function returns true and
  // will continue to return true for the field, even when the type is changed
  // back to 'search'.
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(),
      "document.getElementById('firstname').type = 'search';"));

  // Regression test for crbug.com/918351 whether the datalist becomes available
  // again.
  FocusFirstNameField();
  SendKeyToPageAndWait(ui::DomKey::ARROW_DOWN,
                       {ObservedUiEvents::kSuggestionShown});
  SendKeyToDataListPopup(ui::DomKey::ARROW_DOWN);
  SendKeyToDataListPopup(ui::DomKey::ENTER);
  // Pressing the down arrow preselects the first item. Pressing it again
  // selects the second item.
  ExpectFieldValue("firstname", "Bob");
}

// Test that a JavaScript oninput event is fired after auto-filling a form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnInputAfterAutofill) {
  static const char kOnInputScript[] =
      "<script>"
      "focused_fired = false;"
      "unfocused_fired = false;"
      "changed_select_fired = false;"
      "unchanged_select_fired = false;"
      "document.getElementById('firstname').oninput = function() {"
      "  focused_fired = true;"
      "};"
      "document.getElementById('lastname').oninput = function() {"
      "  unfocused_fired = true;"
      "};"
      "document.getElementById('state').oninput = function() {"
      "  changed_select_fired = true;"
      "};"
      "document.getElementById('country').oninput = function() {"
      "  unchanged_select_fired = true;"
      "};"
      "document.getElementById('country').value = 'US';"
      "</script>";

  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kOnInputScript}));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke Autofill.
  FocusFirstNameField();

  // Start filling the first name field with "M" and wait for the popup to be
  // shown.
  SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                       ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  // The form should be filled.
  ExpectFilledTestForm();

  bool focused_fired = false;
  bool unfocused_fired = false;
  bool changed_select_fired = false;
  bool unchanged_select_fired = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(focused_fired);",
      &focused_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(unfocused_fired);",
      &unfocused_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(changed_select_fired);",
      &changed_select_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(unchanged_select_fired);",
      &unchanged_select_fired));
  EXPECT_TRUE(focused_fired);
  EXPECT_TRUE(unfocused_fired);
  EXPECT_TRUE(changed_select_fired);
  EXPECT_FALSE(unchanged_select_fired);
}

// Test that a JavaScript onchange event is fired after auto-filling a form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnChangeAfterAutofill) {
  static const char kOnChangeScript[] =
      "<script>"
      "focused_fired = false;"
      "unfocused_fired = false;"
      "changed_select_fired = false;"
      "unchanged_select_fired = false;"
      "document.getElementById('firstname').onchange = function() {"
      "  focused_fired = true;"
      "};"
      "document.getElementById('lastname').onchange = function() {"
      "  unfocused_fired = true;"
      "};"
      "document.getElementById('state').onchange = function() {"
      "  changed_select_fired = true;"
      "};"
      "document.getElementById('country').onchange = function() {"
      "  unchanged_select_fired = true;"
      "};"
      "document.getElementById('country').value = 'US';"
      "</script>";

  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kOnChangeScript}));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke Autofill.
  FocusFirstNameField();

  // Start filling the first name field with "M" and wait for the popup to be
  // shown.
  SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                       ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  // The form should be filled.
  ExpectFilledTestForm();

  bool focused_fired = false;
  bool unfocused_fired = false;
  bool changed_select_fired = false;
  bool unchanged_select_fired = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(focused_fired);",
      &focused_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(unfocused_fired);",
      &unfocused_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(changed_select_fired);",
      &changed_select_fired));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(unchanged_select_fired);",
      &unchanged_select_fired));
  EXPECT_TRUE(focused_fired);
  EXPECT_TRUE(unfocused_fired);
  EXPECT_TRUE(changed_select_fired);
  EXPECT_FALSE(unchanged_select_fired);
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, InputFiresBeforeChange) {
  static const char kInputFiresBeforeChangeScript[] =
      "<script>"
      "inputElementEvents = [];"
      "function recordInputElementEvent(e) {"
      "  if (e.target.tagName != 'INPUT') throw 'only <input> tags allowed';"
      "  inputElementEvents.push(e.type);"
      "}"
      "selectElementEvents = [];"
      "function recordSelectElementEvent(e) {"
      "  if (e.target.tagName != 'SELECT') throw 'only <select> tags allowed';"
      "  selectElementEvents.push(e.type);"
      "}"
      "document.getElementById('lastname').oninput = recordInputElementEvent;"
      "document.getElementById('lastname').onchange = recordInputElementEvent;"
      "document.getElementById('country').oninput = recordSelectElementEvent;"
      "document.getElementById('country').onchange = recordSelectElementEvent;"
      "</script>";

  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(
      base::StrCat({kTestShippingFormString, kInputFiresBeforeChangeScript}));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke and accept the Autofill popup and verify the form was filled.
  FocusFirstNameField();
  SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                       ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});
  ExpectFilledTestForm();

  int num_input_element_events = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      GetWebContents(),
      "domAutomationController.send(inputElementEvents.length);",
      &num_input_element_events));
  EXPECT_EQ(2, num_input_element_events);

  std::vector<std::string> input_element_events;
  input_element_events.resize(2);

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetWebContents(), "domAutomationController.send(inputElementEvents[0]);",
      &input_element_events[0]));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetWebContents(), "domAutomationController.send(inputElementEvents[1]);",
      &input_element_events[1]));

  EXPECT_EQ("input", input_element_events[0]);
  EXPECT_EQ("change", input_element_events[1]);

  int num_select_element_events = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      GetWebContents(),
      "domAutomationController.send(selectElementEvents.length);",
      &num_select_element_events));
  EXPECT_EQ(2, num_select_element_events);

  std::vector<std::string> select_element_events;
  select_element_events.resize(2);

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetWebContents(), "domAutomationController.send(selectElementEvents[0]);",
      &select_element_events[0]));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetWebContents(), "domAutomationController.send(selectElementEvents[1]);",
      &select_element_events[1]));

  EXPECT_EQ("input", select_element_events[0]);
  EXPECT_EQ("change", select_element_events[1]);
}

// Test that we can autofill forms distinguished only by their |id| attribute.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       AutofillFormsDistinguishedById) {
  static const char kScript[] =
      "<script>"
      "var mainForm = document.forms[0];"
      "mainForm.id = 'mainForm';"
      "var newForm = document.createElement('form');"
      "newForm.action = mainForm.action;"
      "newForm.method = mainForm.method;"
      "newForm.id = 'newForm';"
      "mainForm.parentNode.insertBefore(newForm, mainForm);"
      "</script>";

  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kScript}));
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that we properly autofill forms with repeated fields.
// In the wild, the repeated fields are typically either email fields
// (duplicated for "confirmation"); or variants that are hot-swapped via
// JavaScript, with only one actually visible at any given time.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillFormWithRepeatedField) {
  static const char kForm[] =
      "<form action=\"http://www.example.com/\" method=\"POST\">"
      "<label for=\"firstname\">First name:</label>"
      " <input type=\"text\" id=\"firstname\""
      "        onfocus=\"domAutomationController.send(true)\"><br>"
      "<label for=\"lastname\">Last name:</label>"
      " <input type=\"text\" id=\"lastname\"><br>"
      "<label for=\"address1\">Address line 1:</label>"
      " <input type=\"text\" id=\"address1\"><br>"
      "<label for=\"address2\">Address line 2:</label>"
      " <input type=\"text\" id=\"address2\"><br>"
      "<label for=\"city\">City:</label>"
      " <input type=\"text\" id=\"city\"><br>"
      "<label for=\"state\">State:</label>"
      " <select id=\"state\">"
      " <option value=\"\" selected=\"yes\">--</option>"
      " <option value=\"CA\">California</option>"
      " <option value=\"TX\">Texas</option>"
      " </select><br>"
      "<label for=\"state_freeform\" style=\"display:none\">State:</label>"
      " <input type=\"text\" id=\"state_freeform\""
      "        style=\"display:none\"><br>"
      "<label for=\"zip\">ZIP code:</label>"
      " <input type=\"text\" id=\"zip\"><br>"
      "<label for=\"country\">Country:</label>"
      " <select id=\"country\">"
      " <option value=\"\" selected=\"yes\">--</option>"
      " <option value=\"CA\">Canada</option>"
      " <option value=\"US\">United States</option>"
      " </select><br>"
      "<label for=\"phone\">Phone number:</label>"
      " <input type=\"text\" id=\"phone\"><br>"
      "</form>";

  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kForm);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke Autofill.
  TryBasicFormFill();
  ExpectFieldValue("state_freeform", std::string());
}

// Test that we properly autofill forms with non-autofillable fields.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       AutofillFormWithNonAutofillableField) {
  static const char kForm[] =
      "<form action=\"http://www.example.com/\" method=\"POST\">"
      "<label for=\"firstname\">First name:</label>"
      " <input type=\"text\" id=\"firstname\""
      "        onfocus=\"domAutomationController.send(true)\"><br>"
      "<label for=\"middlename\">Middle name:</label>"
      " <input type=\"text\" id=\"middlename\" autocomplete=\"off\" /><br>"
      "<label for=\"lastname\">Last name:</label>"
      " <input type=\"text\" id=\"lastname\"><br>"
      "<label for=\"address1\">Address line 1:</label>"
      " <input type=\"text\" id=\"address1\"><br>"
      "<label for=\"address2\">Address line 2:</label>"
      " <input type=\"text\" id=\"address2\"><br>"
      "<label for=\"city\">City:</label>"
      " <input type=\"text\" id=\"city\"><br>"
      "<label for=\"state\">State:</label>"
      " <select id=\"state\">"
      " <option value=\"\" selected=\"yes\">--</option>"
      " <option value=\"CA\">California</option>"
      " <option value=\"TX\">Texas</option>"
      " </select><br>"
      "<label for=\"zip\">ZIP code:</label>"
      " <input type=\"text\" id=\"zip\"><br>"
      "<label for=\"country\">Country:</label>"
      " <select id=\"country\">"
      " <option value=\"\" selected=\"yes\">--</option>"
      " <option value=\"CA\">Canada</option>"
      " <option value=\"US\">United States</option>"
      " </select><br>"
      "<label for=\"phone\">Phone number:</label>"
      " <input type=\"text\" id=\"phone\"><br>"
      "</form>";

  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kForm);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that we can Autofill dynamically generated forms.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DynamicFormFill) {
  static const char kDynamicForm[] =
      "<form id=\"form\" action=\"http://www.example.com/\""
      "      method=\"POST\"></form>"
      "<script>"
      "function AddElement(name, label) {"
      "  var form = document.getElementById('form');"
      ""
      "  var label_text = document.createTextNode(label);"
      "  var label_element = document.createElement('label');"
      "  label_element.setAttribute('for', name);"
      "  label_element.appendChild(label_text);"
      "  form.appendChild(label_element);"
      ""
      "  if (name === 'state' || name === 'country') {"
      "    var select_element = document.createElement('select');"
      "    select_element.setAttribute('id', name);"
      "    select_element.setAttribute('name', name);"
      ""
      "    /* Add an empty selected option. */"
      "    var default_option = new Option('--', '', true);"
      "    select_element.appendChild(default_option);"
      ""
      "    /* Add the other options. */"
      "    if (name == 'state') {"
      "      var option1 = new Option('California', 'CA');"
      "      select_element.appendChild(option1);"
      "      var option2 = new Option('Texas', 'TX');"
      "      select_element.appendChild(option2);"
      "    } else {"
      "      var option1 = new Option('Canada', 'CA');"
      "      select_element.appendChild(option1);"
      "      var option2 = new Option('United States', 'US');"
      "      select_element.appendChild(option2);"
      "    }"
      ""
      "    form.appendChild(select_element);"
      "  } else {"
      "    var input_element = document.createElement('input');"
      "    input_element.setAttribute('id', name);"
      "    input_element.setAttribute('name', name);"
      ""
      "    /* Add the onfocus listener to the 'firstname' field. */"
      "    if (name === 'firstname') {"
      "      input_element.onfocus = function() {"
      "        domAutomationController.send(true);"
      "      };"
      "    }"
      ""
      "    form.appendChild(input_element);"
      "  }"
      ""
      "  form.appendChild(document.createElement('br'));"
      "};"
      ""
      "function BuildForm() {"
      "  var elements = ["
      "    ['firstname', 'First name:'],"
      "    ['lastname', 'Last name:'],"
      "    ['address1', 'Address line 1:'],"
      "    ['address2', 'Address line 2:'],"
      "    ['city', 'City:'],"
      "    ['state', 'State:'],"
      "    ['zip', 'ZIP code:'],"
      "    ['country', 'Country:'],"
      "    ['phone', 'Phone number:'],"
      "  ];"
      ""
      "  for (var i = 0; i < elements.length; i++) {"
      "    var name = elements[i][0];"
      "    var label = elements[i][1];"
      "    AddElement(name, label);"
      "  }"
      "};"
      "</script>";

  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kDynamicForm);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Dynamically construct the form.
  ASSERT_TRUE(content::ExecuteScript(GetWebContents(), "BuildForm();"));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that form filling works after reloading the current page.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillAfterReload) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Reload the page.
  content::WebContents* web_contents = GetWebContents();
  web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
  content::WaitForLoadStop(web_contents);

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that filling a form sends all the expected events to the different
// fields being filled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillEvents) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestEventFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke Autofill.
  TryBasicFormFill();

  // Checks that all the events were fired for the input field.
  bool input_focus_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(inputfocus);",
      &input_focus_triggered));
  EXPECT_TRUE(input_focus_triggered);
  bool input_keydown_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(inputkeydown);",
      &input_keydown_triggered));
  EXPECT_TRUE(input_keydown_triggered);
  bool input_input_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(inputinput);",
      &input_input_triggered));
  EXPECT_TRUE(input_input_triggered);
  bool input_change_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(inputchange);",
      &input_change_triggered));
  EXPECT_TRUE(input_change_triggered);
  bool input_keyup_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(inputkeyup);",
      &input_keyup_triggered));
  EXPECT_TRUE(input_keyup_triggered);
  bool input_blur_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(inputblur);",
      &input_blur_triggered));
  EXPECT_TRUE(input_blur_triggered);

  // Checks that all the events were fired for the textarea field.
  bool text_focus_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(textfocus);",
      &text_focus_triggered));
  EXPECT_TRUE(text_focus_triggered);
  bool text_keydown_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(textkeydown);",
      &text_keydown_triggered));
  EXPECT_TRUE(text_keydown_triggered);
  bool text_input_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(textinput);",
      &text_input_triggered));
  EXPECT_TRUE(text_input_triggered);
  bool text_change_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(textchange);",
      &text_change_triggered));
  EXPECT_TRUE(text_change_triggered);
  bool text_keyup_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(textkeyup);",
      &text_keyup_triggered));
  EXPECT_TRUE(text_keyup_triggered);
  bool text_blur_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(textblur);",
      &text_blur_triggered));
  EXPECT_TRUE(text_blur_triggered);

  // Checks that all the events were fired for the select field.
  bool select_focus_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(selectfocus);",
      &select_focus_triggered));
  EXPECT_TRUE(select_focus_triggered);
  bool select_input_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(selectinput);",
      &select_input_triggered));
  EXPECT_TRUE(select_input_triggered);
  bool select_change_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(selectchange);",
      &select_change_triggered));
  EXPECT_TRUE(select_change_triggered);
  bool select_blur_triggered;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "domAutomationController.send(selectblur);",
      &select_blur_triggered));
  EXPECT_TRUE(select_blur_triggered);
}

// Test fails on Linux ASAN, see http://crbug.com/532737
#if defined(ADDRESS_SANITIZER)
#define MAYBE_AutofillAfterTranslate DISABLED_AutofillAfterTranslate
#else
#define MAYBE_AutofillAfterTranslate AutofillAfterTranslate
#endif  // ADDRESS_SANITIZER
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_AutofillAfterTranslate) {
  ASSERT_TRUE(TranslateService::IsTranslateBubbleEnabled());

  translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);

  CreateTestProfile();

  static const char kForm[] =
      "<form action=\"http://www.example.com/\" method=\"POST\">"
      "<label for=\"fn\">なまえ</label>"
      " <input type=\"text\" id=\"fn\""
      "        onfocus=\"domAutomationController.send(true)\""
      "><br>"
      "<label for=\"ln\">みょうじ</label>"
      " <input type=\"text\" id=\"ln\"><br>"
      "<label for=\"a1\">Address line 1:</label>"
      " <input type=\"text\" id=\"a1\"><br>"
      "<label for=\"a2\">Address line 2:</label>"
      " <input type=\"text\" id=\"a2\"><br>"
      "<label for=\"ci\">City:</label>"
      " <input type=\"text\" id=\"ci\"><br>"
      "<label for=\"st\">State:</label>"
      " <select id=\"st\">"
      " <option value=\"\" selected=\"yes\">--</option>"
      " <option value=\"CA\">California</option>"
      " <option value=\"TX\">Texas</option>"
      " </select><br>"
      "<label for=\"z\">ZIP code:</label>"
      " <input type=\"text\" id=\"z\"><br>"
      "<label for=\"co\">Country:</label>"
      " <select id=\"co\">"
      " <option value=\"\" selected=\"yes\">--</option>"
      " <option value=\"CA\">Canada</option>"
      " <option value=\"US\">United States</option>"
      " </select><br>"
      "<label for=\"ph\">Phone number:</label>"
      " <input type=\"text\" id=\"ph\"><br>"
      "</form>"
      // Add additional Japanese characters to ensure the translate bar
      // will appear.
      "我々は重要な、興味深いものになるが、時折状況が発生するため苦労や痛みは"
      "彼にいくつかの素晴らしいを調達することができます。それから、いくつかの"
      "利";

  // Set up an observer to be able to wait for the bubble to be shown.
  translate::TranslateWaiter language_waiter(
      GetWebContents(),
      translate::TranslateWaiter::WaitEvent::kLanguageDetermined);

  SetTestUrlResponse(kForm);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  language_waiter.Wait();

  // Verify current translate step.
  const TranslateBubbleModel* model =
      translate::test_utils::GetCurrentModel(browser());
  ASSERT_NE(nullptr, model);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            model->GetViewState());

  translate::test_utils::PressTranslate(browser());

  // Wait for translation.
  translate::TranslateWaiter translate_waiter(
      GetWebContents(), translate::TranslateWaiter::WaitEvent::kPageTranslated);

  // Simulate the translate script being retrieved.
  // Pass fake google.translate lib as the translate script.
  SimulateURLFetch();

  translate_waiter.Wait();

  TryBasicFormFill();
}

// Test phone fields parse correctly from a given profile.
// The high level key presses execute the following: Select the first text
// field, invoke the autofill popup list, select the first profile within the
// list, and commit to the profile to populate the form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ComparePhoneNumbers) {
  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 H St."));
  profile.SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("San Jose"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("95110"));
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1-408-555-4567"));
  SetTestProfile(browser(), profile);

  GURL url = embedded_test_server()->GetURL("/autofill/form_phones.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST1");

  ExpectFieldValue("NAME_FIRST1", "Bob");
  ExpectFieldValue("NAME_LAST1", "Smith");
  ExpectFieldValue("ADDRESS_HOME_LINE1", "1234 H St.");
  ExpectFieldValue("ADDRESS_HOME_CITY", "San Jose");
  ExpectFieldValue("ADDRESS_HOME_STATE", "CA");
  ExpectFieldValue("ADDRESS_HOME_ZIP", "95110");
  ExpectFieldValue("PHONE_HOME_WHOLE_NUMBER", "14085554567");

  PopulateForm("NAME_FIRST2");
  ExpectFieldValue("NAME_FIRST2", "Bob");
  ExpectFieldValue("NAME_LAST2", "Smith");
  ExpectFieldValue("PHONE_HOME_CITY_CODE-1", "408");
  ExpectFieldValue("PHONE_HOME_NUMBER", "5554567");

  PopulateForm("NAME_FIRST3");
  ExpectFieldValue("NAME_FIRST3", "Bob");
  ExpectFieldValue("NAME_LAST3", "Smith");
  ExpectFieldValue("PHONE_HOME_CITY_CODE-2", "408");
  ExpectFieldValue("PHONE_HOME_NUMBER_3-1", "555");
  ExpectFieldValue("PHONE_HOME_NUMBER_4-1", "4567");
  ExpectFieldValue("PHONE_HOME_EXT-1", std::string());

  PopulateForm("NAME_FIRST4");
  ExpectFieldValue("NAME_FIRST4", "Bob");
  ExpectFieldValue("NAME_LAST4", "Smith");
  ExpectFieldValue("PHONE_HOME_COUNTRY_CODE-1", "1");
  ExpectFieldValue("PHONE_HOME_CITY_CODE-3", "408");
  ExpectFieldValue("PHONE_HOME_NUMBER_3-2", "555");
  ExpectFieldValue("PHONE_HOME_NUMBER_4-2", "4567");
  ExpectFieldValue("PHONE_HOME_EXT-2", std::string());
}

// Test that Autofill does not fill in Company Name if disabled
IN_PROC_BROWSER_TEST_P(AutofillCompanyInteractiveTest,
                       NoAutofillForCompanyName) {
  std::string addr_line1("1234 H St.");
  std::string company_name("Company X");

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("bsmith@gmail.com"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16(addr_line1));
  profile.SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("San Jose"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("95110"));
  profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16(company_name));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("408-871-4567"));
  SetTestProfile(browser(), profile);

  GURL url =
      embedded_test_server()->GetURL("/autofill/read_only_field_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("firstname");

  ExpectFieldValue("address", addr_line1);
  ExpectFieldValue("company", company_name_enabled_ ? company_name : "");
}

// Test that Autofill does not fill in Company Name if disabled
IN_PROC_BROWSER_TEST_P(AutofillCompanyInteractiveTest,
                       NoAutofillSugggestionForCompanyName) {
  CreateTestProfile();

  std::string company_name("Initech");

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormWithCompanyString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Focus the company field.
  FocusFieldByName("company");

  // Now click it.
  test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionShown},
                                   base::TimeDelta::FromSeconds(3));
  ASSERT_NO_FATAL_FAILURE(ClickElementWithId("company"));

  bool found = test_delegate()->Wait();

  if (!company_name_enabled_) {
    EXPECT_FALSE(found);
    return;
  }
  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  // The form should be filled.
  ExpectFieldValue("company", company_name);
}

// Test that Autofill does not fill in read-only fields.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, NoAutofillForReadOnlyFields) {
  std::string addr_line1("1234 H St.");

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("bsmith@gmail.com"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16(addr_line1));
  profile.SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("San Jose"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("95110"));
  profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Company X"));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("408-871-4567"));
  SetTestProfile(browser(), profile);

  GURL url =
      embedded_test_server()->GetURL("/autofill/read_only_field_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("firstname");

  ExpectFieldValue("email", std::string());
  ExpectFieldValue("address", addr_line1);
}

// Test form is fillable from a profile after form was reset.
// Steps:
//   1. Fill form using a saved profile.
//   2. Reset the form.
//   3. Fill form using a saved profile.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, FormFillableOnReset) {
  CreateTestProfile();

  GURL url =
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ASSERT_TRUE(content::ExecuteScript(
       GetWebContents(), "document.getElementById('testform').reset()"));

  PopulateForm("NAME_FIRST");

  ExpectFieldValue("NAME_FIRST", "Milton");
  ExpectFieldValue("NAME_LAST", "Waddams");
  ExpectFieldValue("EMAIL_ADDRESS", "red.swingline@initech.com");
  ExpectFieldValue("ADDRESS_HOME_LINE1", "4120 Freidrich Lane");
  ExpectFieldValue("ADDRESS_HOME_CITY", "Austin");
  ExpectFieldValue("ADDRESS_HOME_STATE", "Texas");
  ExpectFieldValue("ADDRESS_HOME_ZIP", "78744");
  ExpectFieldValue("ADDRESS_HOME_COUNTRY", "United States");
  ExpectFieldValue("PHONE_HOME_WHOLE_NUMBER", "15125551234");
}

// Test Autofill distinguishes a middle initial in a name.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DistinguishMiddleInitialWithinName) {
  CreateTestProfile();

  GURL url =
      embedded_test_server()->GetURL("/autofill/autofill_middleinit_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ExpectFieldValue("NAME_MIDDLE", "C");
}

// Test forms with multiple email addresses are filled properly.
// Entire form should be filled with one user gesture.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       MultipleEmailFilledByOneUserGesture) {
  std::string email("bsmith@gmail.com");

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(email));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("4088714567"));
  SetTestProfile(browser(), profile);

  GURL url = embedded_test_server()->GetURL(
      "/autofill/autofill_confirmemail_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ExpectFieldValue("EMAIL_CONFIRM", email);
  // TODO(isherman): verify entire form.
}

// Test latency time on form submit with lots of stored Autofill profiles.
// This test verifies when a profile is selected from the Autofill dictionary
// that consists of thousands of profiles, the form does not hang after being
// submitted.
// Flakily times out creating 1500 profiles: http://crbug.com/281527
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DISABLED_FormFillLatencyAfterSubmit) {
  std::vector<std::string> cities;
  cities.push_back("San Jose");
  cities.push_back("San Francisco");
  cities.push_back("Sacramento");
  cities.push_back("Los Angeles");

  std::vector<std::string> streets;
  streets.push_back("St");
  streets.push_back("Ave");
  streets.push_back("Ln");
  streets.push_back("Ct");

  const int kNumProfiles = 1500;
  std::vector<AutofillProfile> profiles;
  for (int i = 0; i < kNumProfiles; i++) {
    AutofillProfile profile;
    base::string16 name(base::NumberToString16(i));
    base::string16 email(name + ASCIIToUTF16("@example.com"));
    base::string16 street =
        ASCIIToUTF16(base::NumberToString(base::RandInt(0, 10000)) + " " +
                     streets[base::RandInt(0, streets.size() - 1)]);
    base::string16 city =
        ASCIIToUTF16(cities[base::RandInt(0, cities.size() - 1)]);
    base::string16 zip(base::NumberToString16(base::RandInt(0, 10000)));
    profile.SetRawInfo(NAME_FIRST, name);
    profile.SetRawInfo(EMAIL_ADDRESS, email);
    profile.SetRawInfo(ADDRESS_HOME_LINE1, street);
    profile.SetRawInfo(ADDRESS_HOME_CITY, city);
    profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
    profile.SetRawInfo(ADDRESS_HOME_ZIP, zip);
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
    profiles.push_back(profile);
  }
  SetTestProfiles(browser(), &profiles);

  GURL url = embedded_test_server()->GetURL(
      "/autofill/latency_after_submit_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  content::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &GetWebContents()->GetController()));

  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(), "document.getElementById('testform').submit();"));
  // This will ensure the test didn't hang.
  load_stop_observer.Wait();
}

// Test that Chrome doesn't crash when autocomplete is disabled while the user
// is interacting with the form.  This is a regression test for
// http://crbug.com/160476
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DisableAutocompleteWhileFilling) {
  CreateTestProfile();

  // Load the test page.
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_NO_FATAL_FAILURE(
      ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Invoke Autofill: Start filling the first name field with "M" and wait for
  // the popup to be shown.
  FocusFirstNameField();
  SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                       ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});

  // Now that the popup with suggestions is showing, disable autocomplete for
  // the active field.
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(),
      "document.querySelector('input').autocomplete = 'off';"));

  // Press the down arrow to select the suggestion and attempt to preview the
  // autofilled form.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});
}

// Test that a page with 2 forms with no name and id containing fields with no
// name or if get filled properly.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       FillFormAndFieldWithNoNameOrId) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "/autofill/forms_without_identifiers.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  // Focus on the first field of the second form.
  bool result = false;
  std::string script =
      R"( function onFocusHandler(e) {
          e.target.removeEventListener(e.type, arguments.callee);
          domAutomationController.send(true);
        }
        if (document.readyState === 'complete') {
          var target = document.forms[1].elements[0];
          target.addEventListener('focus', onFocusHandler);
          target.focus();
        } else {
          domAutomationController.send(false);
        })";
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractBool(GetWebContents(), script, &result));
  ASSERT_TRUE(result);

  // Start filling the first name field with "M" and wait for the popup to be
  // shown.
  SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                       ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});

  // Press Enter to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  // Make sure that the form was filled.
  std::string value;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetWebContents(),
      "window.domAutomationController.send("
      "    document.forms[1].elements[0].value);",
      &value));
  EXPECT_EQ("Milton C. Waddams", value) << "for first field";

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetWebContents(),
      "window.domAutomationController.send("
      "    document.forms[1].elements[1].value);",
      &value));
  EXPECT_EQ("red.swingline@initech.com", value) << "for second field";
}

// The following four tests verify that we can autofill forms with multiple
// nameless forms, and repetitive field names and make sure that the dynamic
// refill would not trigger a wrong refill, regardless of the form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       Dynamic_MultipleNoNameForms_BadNames_FourthForm) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/multiple_noname_forms_badnames.html");

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname_4");
  DoNothingAndWait(2);  // Wait to make sure possible refills have happened.
  // Make sure the correct form was filled.
  ExpectFieldValue("firstname_1", "");
  ExpectFieldValue("lastname_1", "");
  ExpectFieldValue("email_1", "");
  ExpectFieldValue("firstname_2", "");
  ExpectFieldValue("lastname_2", "");
  ExpectFieldValue("email_2", "");
  ExpectFieldValue("firstname_3", "");
  ExpectFieldValue("lastname_3", "");
  ExpectFieldValue("email_3", "");
  ExpectFieldValue("firstname_4", "Milton");
  ExpectFieldValue("lastname_4", "Waddams");
  ExpectFieldValue("email_4", "red.swingline@initech.com");
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       Dynamic_MultipleNoNameForms_BadNames_ThirdForm) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/multiple_noname_forms_badnames.html");

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname_3");
  DoNothingAndWait(2);  // Wait to make sure possible refills have happened.
  // Make sure the correct form was filled.
  ExpectFieldValue("firstname_1", "");
  ExpectFieldValue("lastname_1", "");
  ExpectFieldValue("email_1", "");
  ExpectFieldValue("firstname_2", "");
  ExpectFieldValue("lastname_2", "");
  ExpectFieldValue("email_2", "");
  ExpectFieldValue("firstname_3", "Milton");
  ExpectFieldValue("lastname_3", "Waddams");
  ExpectFieldValue("email_3", "red.swingline@initech.com");
  ExpectFieldValue("firstname_4", "");
  ExpectFieldValue("lastname_4", "");
  ExpectFieldValue("email_4", "");
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       Dynamic_MultipleNoNameForms_BadNames_SecondForm) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/multiple_noname_forms_badnames.html");

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname_2");
  DoNothingAndWait(2);  // Wait to make sure possible refills have happened.
  // Make sure the correct form was filled.
  ExpectFieldValue("firstname_1", "");
  ExpectFieldValue("lastname_1", "");
  ExpectFieldValue("email_1", "");
  ExpectFieldValue("firstname_2", "Milton");
  ExpectFieldValue("lastname_2", "Waddams");
  ExpectFieldValue("email_2", "red.swingline@initech.com");
  ExpectFieldValue("firstname_3", "");
  ExpectFieldValue("lastname_3", "");
  ExpectFieldValue("email_3", "");
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       Dynamic_MultipleNoNameForms_BadNames_FirstForm) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/multiple_noname_forms_badnames.html");

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname_1");
  DoNothingAndWait(2);  // Wait to make sure possible refills have happened.
  // Make sure the correct form was filled.
  ExpectFieldValue("firstname_1", "Milton");
  ExpectFieldValue("lastname_1", "Waddams");
  ExpectFieldValue("email_1", "red.swingline@initech.com");
  ExpectFieldValue("firstname_2", "");
  ExpectFieldValue("lastname_2", "");
  ExpectFieldValue("email_2", "");
  ExpectFieldValue("firstname_3", "");
  ExpectFieldValue("lastname_3", "");
  ExpectFieldValue("email_3", "");
}

// Test that we can Autofill forms where some fields name change during the
// fill.
IN_PROC_BROWSER_TEST_P(AutofillCompanyInteractiveTest, FieldsChangeName) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "/autofill/field_changing_name_during_fill.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the fill to happen.
  bool has_filled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(GetWebContents(),
                                                   "hasFilled()", &has_filled));
  ASSERT_TRUE(has_filled);

  // Make sure the form was filled correctly.
  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("address", "4120 Freidrich Lane");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

class AutofillCreditCardInteractiveTest : public AutofillInteractiveTestBase {
 protected:
  AutofillCreditCardInteractiveTest() = default;
  ~AutofillCreditCardInteractiveTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillInteractiveTestBase::SetUpCommandLine(command_line);
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "a.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  // After autofilling the credit card, there is a delayed task of recording its
  // use on the db. If we reenable the services, the config would be deleted and
  // we won't be able to encrypt the cc number. There will be a crash while
  // encrypting the cc number.
  void TearDownOnMainThread() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardInteractiveTest);
};

// Test that credit card autofill works.
IN_PROC_BROWSER_TEST_F(AutofillCreditCardInteractiveTest, FillLocalCreditCard) {
  CreateTestCreditCart();

  // Navigate to the page.
  GURL url = https_server()->GetURL("a.com",
                                    "/autofill/autofill_creditcard_form.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  // Trigger the autofill.
  FocusFieldByName("CREDIT_CARD_NAME_FULL");
  AcceptSuggestionUsingArrowDown();

  ExpectFieldValue("CREDIT_CARD_NAME_FULL", "Milton Waddams");
  ExpectFieldValue("CREDIT_CARD_NUMBER", "4111111111111111");
  ExpectFieldValue("CREDIT_CARD_EXP_MONTH", "09");
  ExpectFieldValue("CREDIT_CARD_EXP_4_DIGIT_YEAR", "2999");
}

// Test params:
//  - bool restrict_unowned_fields_: whether autofill of unowned fields is
//        restricted to checkout related pages.
class AutofillRestrictUnownedFieldsTest
    : public AutofillInteractiveTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  AutofillRestrictUnownedFieldsTest()
      : restrict_unowned_fields_(std::get<0>(GetParam())),
        autofill_enable_company_name_(std::get<1>(GetParam())) {
    std::vector<base::Feature> enabled;
    std::vector<base::Feature> disabled = {
        features::kAutofillEnforceMinRequiredFieldsForHeuristics,
        features::kAutofillEnforceMinRequiredFieldsForQuery,
        features::kAutofillEnforceMinRequiredFieldsForUpload};
    (restrict_unowned_fields_ ? enabled : disabled)
        .push_back(features::kAutofillRestrictUnownedFieldsToFormlessCheckout);
    (autofill_enable_company_name_ ? enabled : disabled)
        .push_back(features::kAutofillEnableCompanyName);
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  const bool restrict_unowned_fields_;
  const bool autofill_enable_company_name_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that we do not fill formless non-checkout forms when we enable the
// formless form restrictions.
IN_PROC_BROWSER_TEST_P(AutofillRestrictUnownedFieldsTest, NoAutocomplete) {
  SCOPED_TRACE(base::StringPrintf("restrict_unowned_fields_ = %d",
                                  restrict_unowned_fields_));
  base::HistogramTester histogram;

  CreateTestProfile();

  GURL url =
      embedded_test_server()->GetURL("/autofill/formless_no_autocomplete.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  // Of unowned forms are restricted, then there are no forms detected.
  if (restrict_unowned_fields_) {
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    // We should only have samples saying that some elements were filtered.
    auto buckets =
        histogram.GetAllSamples("Autofill.UnownedFieldsWereFiltered");
    ASSERT_EQ(1u, buckets.size());
    EXPECT_EQ(1, buckets[0].min);  // The "true" bucket.

    ASSERT_EQ(0U, GetAutofillManager()->NumFormsDetected());
    return;
  }

  // If we reach this point, then unowned forms are not restricted. There
  // should a form we can trigger fill on (using the firstname field)
  ASSERT_FALSE(restrict_unowned_fields_);
  ASSERT_EQ(1U, GetAutofillManager()->NumFormsDetected());
  TriggerFormFill("firstname");

  // Wait for the fill to happen.
  bool has_filled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(GetWebContents(),
                                                   "hasFilled()", &has_filled));
  EXPECT_EQ(has_filled, !restrict_unowned_fields_);

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // If only some form fields are tagged with autocomplete types, then the
  // number of input elements will not match the number of fields when autofill
  // triees to preview or fill.
  histogram.ExpectUniqueSample("Autofill.NumElementsMatchesNumFields",
                               !restrict_unowned_fields_, 2);

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("address", "4120 Freidrich Lane");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", autofill_enable_company_name_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that we do not fill formless non-checkout forms when we enable the
// formless form restrictions. This test differes from the NoAutocomplete
// version of the the test in that at least one of the fields has an
// autocomplete attribute, so autofill will always be aware of the existence
// of the form.
IN_PROC_BROWSER_TEST_P(AutofillRestrictUnownedFieldsTest, SomeAutocomplete) {
  SCOPED_TRACE(base::StringPrintf("restrict_unowned_fields_ = %d",
                                  restrict_unowned_fields_));
  CreateTestProfile();

  base::HistogramTester histogram;

  GURL url = embedded_test_server()->GetURL(
      "/autofill/formless_some_autocomplete.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_EQ(1U, GetAutofillManager()->NumFormsDetected());
  TriggerFormFill("firstname");

  // Wait for the fill to happen.
  bool has_filled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(GetWebContents(),
                                                   "hasFilled()", &has_filled));
  EXPECT_EQ(has_filled, !restrict_unowned_fields_);

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // If only some form fields are tagged with autocomplete types, then the
  // number of input elements will not match the number of fields when autofill
  // triees to preview or fill.
  histogram.ExpectUniqueSample("Autofill.NumElementsMatchesNumFields",
                               !restrict_unowned_fields_, 2);

  // http://crbug.com/841784
  // Formless fields with autocomplete attributes don't work because the
  // extracted form and the form to be previewed/filled end up with a mismatched
  // number of fields and early abort.
  // This is fixed when !restrict_unowned_fields_
  if (restrict_unowned_fields_) {
    // We should only have samples saying that some elements were filtered.
    auto buckets =
        histogram.GetAllSamples("Autofill.UnownedFieldsWereFiltered");
    ASSERT_EQ(1u, buckets.size());
    EXPECT_EQ(1, buckets[0].min);  // The "true" bucket.

    ExpectFieldValue("firstname", "M");
    ExpectFieldValue("address", "");
    ExpectFieldValue("state", "--");
    ExpectFieldValue("city", "");
    ExpectFieldValue("company", "");
    ExpectFieldValue("email", "");
    ExpectFieldValue("phone", "");
  } else {
    ExpectFieldValue("firstname", "Milton");
    ExpectFieldValue("address", "4120 Freidrich Lane");
    ExpectFieldValue("state", "TX");
    ExpectFieldValue("city", "Austin");
    ExpectFieldValue("company", autofill_enable_company_name_ ? "Initech" : "");
    ExpectFieldValue("email", "red.swingline@initech.com");
    ExpectFieldValue("phone", "15125551234");
  }
}

// Test that we do not fill formless non-checkout forms when we enable the
// formless form restrictions.
IN_PROC_BROWSER_TEST_P(AutofillRestrictUnownedFieldsTest, AllAutocomplete) {
  SCOPED_TRACE(base::StringPrintf("restrict_unowned_fields_ = %d",
                                  restrict_unowned_fields_));
  CreateTestProfile();

  base::HistogramTester histogram;

  GURL url = embedded_test_server()->GetURL(
      "/autofill/formless_all_autocomplete.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_EQ(1U, GetAutofillManager()->NumFormsDetected());
  TriggerFormFill("firstname");

  // Wait for the fill to happen.
  bool has_filled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(GetWebContents(),
                                                   "hasFilled()", &has_filled));
  EXPECT_TRUE(has_filled);

  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // If all form fields are tagged with autocomplete types, we make them all
  // available to be filled.
  histogram.ExpectUniqueSample("Autofill.NumElementsMatchesNumFields", true, 2);

  if (restrict_unowned_fields_) {
    // We should only have samples saying that no elements were filtered.
    auto buckets =
        histogram.GetAllSamples("Autofill.UnownedFieldsWereFiltered");
    ASSERT_EQ(1u, buckets.size());
    EXPECT_EQ(0, buckets[0].min);  // The "false" bucket.
  }

  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("address", "4120 Freidrich Lane");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", autofill_enable_company_name_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// An extension of the test fixture for tests with site isolation.
//
// Test params:
//  - bool popup_views_enabled: whether feature AutofillExpandedPopupViews
//        is enabled for testing.
class AutofillInteractiveIsolationTest : public AutofillInteractiveTestBase {
 protected:
  AutofillInteractiveIsolationTest() = default;
  ~AutofillInteractiveIsolationTest() override = default;

  bool IsPopupShown() {
    return !!static_cast<ChromeAutofillClient*>(
                 ContentAutofillDriverFactory::FromWebContents(GetWebContents())
                     ->DriverForFrame(GetWebContents()->GetMainFrame())
                     ->autofill_manager()
                     ->client())
                 ->popup_controller_for_testing();
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillInteractiveTestBase::SetUpCommandLine(command_line);
    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(AutofillInteractiveIsolationTest, SimpleCrossSiteFill) {
  CreateTestProfile();

  // Main frame is on a.com, iframe is on b.com.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/cross_origin_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill/autofill_test_form.html");
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "crossFrame", iframe_url));

  // Let |test_delegate()| also observe autofill events in the iframe.
  content::RenderFrameHost* cross_frame =
      RenderFrameHostForName(GetWebContents(), "crossFrame");
  ASSERT_TRUE(cross_frame);
  ContentAutofillDriver* cross_driver =
      ContentAutofillDriverFactory::FromWebContents(GetWebContents())
          ->DriverForFrame(cross_frame);
  ASSERT_TRUE(cross_driver);
  cross_driver->autofill_manager()->SetTestDelegate(test_delegate());

  // Focus the form in the iframe and simulate choosing a suggestion via
  // keyboard.
  std::string script_focus("document.getElementById('NAME_FIRST').focus();");
  ASSERT_TRUE(content::ExecuteScript(cross_frame, script_focus));
  content::RenderWidgetHost* widget =
      cross_frame->GetView()->GetRenderWidgetHost();
  AcceptSuggestionUsingArrowDown(/*suggestion_position=*/1, widget);

  // Check that the suggestion was filled.
  std::string value;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      cross_frame,
      "window.domAutomationController.send("
      "    document.getElementById('NAME_FIRST').value);",
      &value));
  EXPECT_EQ("Milton", value);
}

// This test verifies that credit card (payment card list) popup works when the
// form is inside an OOPIF.
// Flaky on Windows http://crbug.com/728488
#if defined(OS_WIN)
#define MAYBE_CrossSitePaymentForms DISABLED_CrossSitePaymentForms
#else
#define MAYBE_CrossSitePaymentForms CrossSitePaymentForms
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, MAYBE_CrossSitePaymentForms) {
  CreateTestCreditCart();
  // Main frame is on a.com, iframe is on b.com.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/cross_origin_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill/autofill_creditcard_form.html");
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "crossFrame", iframe_url));

  // Let |test_delegate()| also observe autofill events in the iframe.
  content::RenderFrameHost* cross_frame =
      RenderFrameHostForName(GetWebContents(), "crossFrame");
  ASSERT_TRUE(cross_frame);
  ContentAutofillDriver* cross_driver =
      ContentAutofillDriverFactory::FromWebContents(GetWebContents())
          ->DriverForFrame(cross_frame);
  ASSERT_TRUE(cross_driver);
  cross_driver->autofill_manager()->SetTestDelegate(test_delegate());

  // Focus the form in the iframe and simulate choosing a suggestion via
  // keyboard.
  std::string script_focus(
      "window.focus();"
      "document.getElementById('CREDIT_CARD_NUMBER').focus();");
  ASSERT_TRUE(content::ExecuteScript(cross_frame, script_focus));

  // Send an arrow dow keypress in order to trigger the autofill popup.
  SendKeyToPageAndWait(ui::DomKey::ARROW_DOWN,
                       {ObservedUiEvents::kSuggestionShown});
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveIsolationTest,
                       DeletingFrameUnderSuggestion) {
  CreateTestProfile();

  // Main frame is on a.com, iframe is on b.com.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/cross_origin_iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill/autofill_test_form.html");
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "crossFrame", iframe_url));

  // Let |test_delegate()| also observe autofill events in the iframe.
  content::RenderFrameHost* cross_frame =
      RenderFrameHostForName(GetWebContents(), "crossFrame");
  ASSERT_TRUE(cross_frame);
  ContentAutofillDriver* cross_driver =
      ContentAutofillDriverFactory::FromWebContents(GetWebContents())
          ->DriverForFrame(cross_frame);
  ASSERT_TRUE(cross_driver);
  cross_driver->autofill_manager()->SetTestDelegate(test_delegate());

  // Focus the form in the iframe and simulate choosing a suggestion via
  // keyboard.
  std::string script_focus("document.getElementById('NAME_FIRST').focus();");
  ASSERT_TRUE(content::ExecuteScript(cross_frame, script_focus));
  SendKeyToPageAndWait(ui::DomKey::ARROW_DOWN,
                       {ObservedUiEvents::kSuggestionShown});
  content::RenderWidgetHost* widget =
      cross_frame->GetView()->GetRenderWidgetHost();
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData}, widget);
  // Do not accept the suggestion yet, to keep the pop-up shown.
  EXPECT_TRUE(IsPopupShown());

  // Delete the iframe.
  std::string script_delete =
      "document.body.removeChild(document.getElementById('crossFrame'));";
  ASSERT_TRUE(content::ExecuteScript(GetWebContents(), script_delete));

  // The popup should have disappeared with the iframe.
  EXPECT_FALSE(IsPopupShown());
}

// Test params:
//  - bool popup_views_enabled: whether feature AutofillExpandedPopupViews
//        is enabled for testing.
class AutofillDynamicFormInteractiveTest
    : public AutofillInteractiveTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  AutofillDynamicFormInteractiveTest()
      : AutofillInteractiveTestBase(), company_name_enabled_(GetParam()) {
    // Setup that the test expects a re-fill to happen.
    test_delegate()->SetIsExpectingDynamicRefill(true);
  }
  ~AutofillDynamicFormInteractiveTest() override = default;

  // AutofillInteractiveTestBase:
  void SetUp() override {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    if (company_name_enabled_) {
      enabled_features.push_back(features::kAutofillEnableCompanyName);
    } else {
      disabled_features.push_back(features::kAutofillEnableCompanyName);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    AutofillInteractiveTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillInteractiveTestBase::SetUpCommandLine(command_line);
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "a.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  const bool company_name_enabled_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDynamicFormInteractiveTest);
};

// Test that we can Autofill dynamically generated forms.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill) {
  CreateTestProfile();

  GURL url =
      embedded_test_server()->GetURL("a.com", "/autofill/dynamic_form.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname_form1", "Milton");
  ExpectFieldValue("address_form1", "4120 Freidrich Lane");
  ExpectFieldValue("state_form1", "TX");
  ExpectFieldValue("city_form1", "Austin");
  ExpectFieldValue("company_form1", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email_form1", "red.swingline@initech.com");
  ExpectFieldValue("phone_form1", "15125551234");
}

IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       TwoDynamicChangingFormsFill) {
  // Setup that the test expects a re-fill to happen.
  test_delegate()->SetIsExpectingDynamicRefill(true);

  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL("a.com",
                                            "/autofill/two_dynamic_forms.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname_form1");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled('firstname_form1')", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname_form1", "Milton");
  ExpectFieldValue("address_form1", "4120 Freidrich Lane");
  ExpectFieldValue("state_form1", "TX");
  ExpectFieldValue("city_form1", "Austin");
  ExpectFieldValue("company_form1", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email_form1", "red.swingline@initech.com");
  ExpectFieldValue("phone_form1", "15125551234");

  TriggerFormFill("firstname_form2");

  // Wait for the re-fill to happen.
  has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled('firstname_form2')", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname_form2", "Milton");
  ExpectFieldValue("address_form2", "4120 Freidrich Lane");
  ExpectFieldValue("state_form2", "TX");
  ExpectFieldValue("city_form2", "Austin");
  ExpectFieldValue("company_form2", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email_form2", "red.swingline@initech.com");
  ExpectFieldValue("phone_form2", "15125551234");
}

// Test that forms that dynamically change a second time do not get filled.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_SecondChange) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/double_dynamic_form.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for two dynamic changes to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_FALSE(has_refilled);

  // Make sure the new form was not filled.
  ExpectFieldValue("firstname_form2", "");
  ExpectFieldValue("address_form2", "");
  ExpectFieldValue("state_form2", "CA");  // Default value.
  ExpectFieldValue("city_form2", "");
  ExpectFieldValue("company_form2", "");
  ExpectFieldValue("email_form2", "");
  ExpectFieldValue("phone_form2", "");
}

// Test that forms that dynamically change after a second do not get filled.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_AfterDelay) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_after_delay.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the dynamic change to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_FALSE(has_refilled);

  // Make sure that the new form was not filled.
  ExpectFieldValue("firstname_form1", "");
  ExpectFieldValue("address_form1", "");
  ExpectFieldValue("state_form1", "CA");  // Default value.
  ExpectFieldValue("city_form1", "");
  ExpectFieldValue("company_form1", "");
  ExpectFieldValue("email_form1", "");
  ExpectFieldValue("phone_form1", "");
}

// Test that only field of a type group that was filled initially get refilled.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_AddsNewFieldTypeGroups) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_new_field_types.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the dynamic change to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // The fields present in the initial fill should be filled.
  ExpectFieldValue("firstname_form1", "Milton");
  ExpectFieldValue("address_form1", "4120 Freidrich Lane");
  ExpectFieldValue("state_form1", "TX");
  ExpectFieldValue("city_form1", "Austin");
  // Fields from group that were not present in the initial fill should not be
  // filled
  ExpectFieldValue("company_form1", "");
  // Fields that were present but hidden in the initial fill should not be
  // filled.
  ExpectFieldValue("email_form1", "");
  // The phone should be filled even if it's a different format than the initial
  // fill.
  ExpectFieldValue("phone_form1", "5125551234");
}

// Test that we can autofill forms that dynamically change select fields to text
// fields by changing the visibilities.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicFormFill_SelectToText) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_select_to_text.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));
  TriggerFormFill("firstname");
  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("state_us", "Texas");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that we can autofill forms that dynamically change the visibility of a
// field after it's autofilled.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicFormFill_VisibilitySwitch) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_visibility_switch.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));
  TriggerFormFill("firstname");
  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  // Both fields must be filled after a refill.
  ExpectFieldValue("state_first", "Texas");
  ExpectFieldValue("state_second", "Texas");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicFormFill_FirstElementDisappears) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_element_invalid.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));
  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname2", "Milton");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though the form has no name.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicFormFill_FirstElementDisappearsNoNameForm) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_element_invalid_noname_form.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));
  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname2", "Milton");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though there are multiple forms with identical
// names.
IN_PROC_BROWSER_TEST_P(
    AutofillDynamicFormInteractiveTest,
    DynamicFormFill_FirstElementDisappearsMultipleBadNameForms) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com",
      "/autofill/dynamic_form_element_invalid_multiple_badname_forms.html");

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname_5");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));

  ASSERT_TRUE(has_refilled);
  // Make sure the second form was filled correctly, and the first form was left
  // unfilled.
  ExpectFieldValue("firstname_1", "");
  ExpectFieldValue("firstname_2", "");
  ExpectFieldValue("address1_3", "");
  ExpectFieldValue("country_4", "CA");  // default
  ExpectFieldValue("firstname_6", "Milton");
  ExpectFieldValue("address1_7", "4120 Freidrich Lane");
  ExpectFieldValue("country_8", "US");
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though there are multiple forms with identical
// names.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicFormFill_FirstElementDisappearsBadnameUnowned) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_element_invalid_unowned_badnames.html");

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname_5");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));

  ASSERT_TRUE(has_refilled);
  // Make sure the second form was filled correctly, and the first form was left
  // unfilled.
  ExpectFieldValue("firstname_1", "");
  ExpectFieldValue("firstname_2", "");
  ExpectFieldValue("address1_3", "");
  ExpectFieldValue("country_4", "CA");  // default
  ExpectFieldValue("firstname_6", "Milton");
  ExpectFieldValue("address1_7", "4120 Freidrich Lane");
  ExpectFieldValue("country_8", "US");
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though there are multiple forms with no name.
IN_PROC_BROWSER_TEST_P(
    AutofillDynamicFormInteractiveTest,
    DynamicFormFill_FirstElementDisappearsMultipleNoNameForms) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com",
      "/autofill/dynamic_form_element_invalid_multiple_noname_forms.html");

  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname_5");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));

  ASSERT_TRUE(has_refilled);
  // Make sure the second form was filled correctly, and the first form was left
  // unfilled.
  ExpectFieldValue("firstname_1", "");
  ExpectFieldValue("firstname_2", "");
  ExpectFieldValue("address1_3", "");
  ExpectFieldValue("country_4", "CA");  // default
  ExpectFieldValue("firstname_6", "Milton");
  ExpectFieldValue("address1_7", "4120 Freidrich Lane");
  ExpectFieldValue("country_8", "US");
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though the elements are unowned.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicFormFill_FirstElementDisappearsUnowned) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_element_invalid_unowned.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));
  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname2", "Milton");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that credit card fields are never re-filled.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_NotForCreditCard) {
  CreateTestCreditCart();

  // Navigate to the page.
  GURL url = https_server()->GetURL("a.com",
                                    "/autofill/dynamic_form_credit_card.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  // Trigger the initial fill.
  FocusFieldByName("cc-name");
  SendKeyToPageAndWait(ui::DomKey::FromCharacter('M'), ui::DomCode::US_M,
                       ui::VKEY_M, {ObservedUiEvents::kSuggestionShown});
  SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN,
                        {ObservedUiEvents::kPreviewFormData});
  SendKeyToPopupAndWait(ui::DomKey::ENTER, {ObservedUiEvents::kFormDataFilled});

  // Wait for the dynamic change to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_FALSE(has_refilled);

  // There should be no values in the fields.
  ExpectFieldValue("cc-name", "");
  ExpectFieldValue("cc-num", "");
  ExpectFieldValue("cc-exp-month", "01");   // Default value.
  ExpectFieldValue("cc-exp-year", "2010");  // Default value.
  ExpectFieldValue("cc-csc", "");
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_SelectUpdated) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_select_options_change.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed only once.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_DoubleSelectUpdated) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_double_select_options_change.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_FALSE(has_refilled);

  // The fields that were initially filled and not reset should still be filled.
  ExpectFieldValue("firstname", "");  // That field value was reset dynamically.
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("state", "CA");   // Default value.
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that we can Autofill dynamically generated forms with no name if the
// NameForAutofill of the first field matches.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_FormWithoutName) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_no_name.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname_form1", "Milton");
  ExpectFieldValue("address_form1", "4120 Freidrich Lane");
  ExpectFieldValue("state_form1", "TX");
  ExpectFieldValue("city_form1", "Austin");
  ExpectFieldValue("company_form1", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email_form1", "red.swingline@initech.com");
  ExpectFieldValue("phone_form1", "15125551234");
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed for forms with no names if the NameForAutofill of the first
// field matches.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_SelectUpdated_FormWithoutName) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com",
      "/autofill/dynamic_form_with_no_name_select_options_change.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

// Test that we can Autofill dynamically generated synthetic forms if the
// NameForAutofill of the first field matches.
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_SyntheticForm) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_synthetic_form.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname_syntheticform1", "Milton");
  ExpectFieldValue("address_syntheticform1", "4120 Freidrich Lane");
  ExpectFieldValue("state_syntheticform1", "TX");
  ExpectFieldValue("city_syntheticform1", "Austin");
  ExpectFieldValue("company_syntheticform1",
                   company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email_syntheticform1", "red.swingline@initech.com");
  ExpectFieldValue("phone_syntheticform1", "15125551234");
}

// Test that we can Autofill dynamically synthetic forms when the select options
// change if the NameForAutofill of the first field matches
IN_PROC_BROWSER_TEST_P(AutofillDynamicFormInteractiveTest,
                       DynamicChangingFormFill_SelectUpdated_SyntheticForm) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_synthetic_form_select_options_change.html");
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  TriggerFormFill("firstname");

  // Wait for the re-fill to happen.
  bool has_refilled = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetWebContents(), "hasRefilled()", &has_refilled));
  ASSERT_TRUE(has_refilled);

  // Make sure the new form was filled correctly.
  ExpectFieldValue("firstname", "Milton");
  ExpectFieldValue("address1", "4120 Freidrich Lane");
  ExpectFieldValue("state", "TX");
  ExpectFieldValue("city", "Austin");
  ExpectFieldValue("company", company_name_enabled_ ? "Initech" : "");
  ExpectFieldValue("email", "red.swingline@initech.com");
  ExpectFieldValue("phone", "15125551234");
}

INSTANTIATE_TEST_SUITE_P(All, AutofillCompanyInteractiveTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillDynamicFormInteractiveTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillRestrictUnownedFieldsTest,
                         testing::Combine(testing::Bool(), testing::Bool()));
}  // namespace autofill
