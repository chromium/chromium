// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"

using base::ASCIIToUTF16;
using testing::AllOf;
using testing::AnyOf;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::Each;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Key;
using testing::Le;
using testing::Ne;
using testing::Optional;
using testing::Pair;
using testing::Pointee;
using testing::Property;
using testing::ResultOf;

namespace autofill {

namespace {

constexpr char kNameFull[] = "Barack Obama";
constexpr char kNumber[] = "4444333322221111";
constexpr char kExpMonth[] = "12";
constexpr char kExpYear[] = "2035";
constexpr char kExp[] = "12/2035";
constexpr char kCvc[] = "123";

// Adds waiting capabilities to BrowserAutofillManager.
class TestAutofillManager : public BrowserAutofillManager {
 public:
  TestAutofillManager(ContentAutofillDriver* driver, AutofillClient* client)
      : BrowserAutofillManager(driver,
                               client,
                               "en-US",
                               EnableDownloadManager(false)) {}

  static TestAutofillManager* GetForRenderFrameHost(
      content::RenderFrameHost* rfh) {
    return static_cast<TestAutofillManager*>(
        ContentAutofillDriver::GetForRenderFrameHost(rfh)->autofill_manager());
  }

  const FormStructure* WaitForMatchingForm(
      base::RepeatingCallback<bool(const FormStructure&)> pred) {
    return ::autofill::WaitForMatchingForm(this, std::move(pred));
  }

  [[nodiscard]] AssertionResult WaitForAutofill(size_t num_awaited_calls) {
    return did_autofill_.Wait(num_awaited_calls);
  }

  [[nodiscard]] AssertionResult WaitForSubmission(size_t num_awaited_calls) {
    return form_submitted_.Wait(num_awaited_calls);
  }

  void OnFormSubmittedImpl(const FormData& form,
                           bool known_success,
                           mojom::SubmissionSource source) override {
    BrowserAutofillManager::OnFormSubmittedImpl(form, known_success, source);
    // The submitted form does not end up in the form cache, so we need to catch
    // it here.
    submitted_form_ = form;
  }

  absl::optional<FormData> submitted_form() const { return submitted_form_; }

 private:
  TestAutofillManagerWaiter did_autofill_{
      *this,
      {&AutofillManager::Observer::OnAfterDidFillAutofillFormData}};
  TestAutofillManagerWaiter form_submitted_{
      *this,
      {&AutofillManager::Observer::OnAfterFormSubmitted}};
  absl::optional<FormData> submitted_form_;
};

// Fakes an Autofill on of a given form.
void FillCard(content::RenderFrameHost* rfh,
              const FormData& form,
              const FormFieldData& triggered_field) {
  CreditCard card;
  test::SetCreditCardInfo(&card, kNameFull, kNumber, kExpMonth, kExpYear, "");
  auto* manager = TestAutofillManager::GetForRenderFrameHost(rfh);
  manager->FillCreditCardFormImpl(form, triggered_field, card,
                                  base::ASCIIToUTF16(base::StringPiece(kCvc)));
}

// Clicks the first input, textarea, or select in `rfh`.
// Returns true if such an element is clickable.
bool ClickFirstField(content::WebContents* web_contents,
                     content::RenderFrameHost* rfh) {
  std::string bounds = R"(
      const bounds = (
        document.querySelector('input')    ||
        document.querySelector('textarea') ||
        document.querySelector('select')   ||
        document.head  // Has all zeros.
      ).getBoundingClientRect();
  )";
  std::string x_script = bounds + "Math.floor(bounds.left + bounds.width / 2)";
  std::string y_script = bounds + "Math.floor(bounds.top + bounds.height / 2)";
  gfx::Point point = rfh->GetView()->TransformPointToRootCoordSpace(
      {content::EvalJs(rfh, x_script).ExtractInt(),
       content::EvalJs(rfh, y_script).ExtractInt()});
  if (point == gfx::Point(0, 0))
    return false;
  content::SimulateMouseClickAt(web_contents, 0,
                                blink::WebMouseEvent::Button::kLeft, point);
  return true;
}

// Returns the values of all fields in the  frames of `web_contents`.
// The values are sorted by DOM order in the respective frame.
std::map<LocalFrameToken, std::vector<std::string>> AllFieldValues(
    content::WebContents* web_contents) {
  constexpr const char kExtractValue[] = R"(
    [...document.querySelectorAll('input, textarea, select')]
      .map(field => field.value)
  )";
  std::map<LocalFrameToken, std::vector<std::string>> values;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        content::EvalJsResult r = content::EvalJs(rfh, kExtractValue);
        if (r.error.empty()) {
          LocalFrameToken frame(rfh->GetFrameToken().value());
          for (const base::Value& value : r.value.GetList())
            values[frame].push_back(value.GetString());
        }
      });
  return values;
}

// Returns the values of all fields in the  frames of `web_contents`.
// The values are sorted according to the ordering of the `form.fields`.
// Returns `{}` if there's a mismatch between the DOM fields and `form.fields`.
std::vector<std::string> AllFieldValues(content::WebContents* web_contents,
                                        const FormData& form) {
  std::map<LocalFrameToken, std::vector<std::string>> frame_to_values =
      AllFieldValues(web_contents);
  std::map<LocalFrameToken, std::vector<std::string>::const_iterator>
      frame_to_iters;
  for (const auto& [frame, frame_values] : frame_to_values)
    frame_to_iters[frame] = frame_values.begin();

  std::vector<std::string> values;
  for (const FormFieldData& field : form.fields) {
    LocalFrameToken frame = field.host_frame;
    if (frame_to_iters[frame] == frame_to_values[frame].end())
      return {};
    values.push_back(*frame_to_iters[frame]++);
  }

  for (const auto& [frame, frame_values] : frame_to_values) {
    if (frame_to_iters[frame] != frame_values.end())
      return {};
  }
  return values;
}

// Matches a `FormStructure` if its field type frequencies are within the limits
// accepted by Autofill.
auto IsWithinAutofillLimits() {
  auto frequencies = [](const FormStructure& form) {
    std::map<ServerFieldType, size_t> counts;
    for (const auto& field : form)
      ++counts[field->Type().GetStorableType()];
    return counts;
  };
  return ResultOf(frequencies,
                  Each(AnyOf(Key(NO_SERVER_DATA), Key(UNKNOWN_TYPE),
                             Pair(Eq(CREDIT_CARD_NUMBER),
                                  Le(kCreditCardTypeValueFormFillingLimit)),
                             Pair(Ne(CREDIT_CARD_NUMBER),
                                  Le(kTypeValueFormFillingLimit)))));
}

auto HasValue(base::StringPiece value) {
  return Field(&FormFieldData::value, base::ASCIIToUTF16(value));
}

}  // namespace

// Test fixture for all tests of AutofillAcrossIframes. A particular goal is is
// to test that ContentAutofillRouter and FormForest handle the race conditions
// that arise during page load correctly; see
// go/autofill-iframes-race-condition-explainer for some explanation.
class AutofillAcrossIframesTest : public InProcessBrowserTest {
 public:
  AutofillAcrossIframesTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillAcrossIframes,
                              features::kAutofillSharedAutofill},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Prevent the Keychain from coming up on Mac.
    test::DisableSystemServices(browser()->profile()->GetPrefs());
    // Set up the HTTPS (!) server (embedded_test_server() is an HTTP server).
    // Every hostname is handled by that server.
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        [](const std::map<std::string, std::string>* pages,
           const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto it = pages->find(request.GetURL().path());
          if (it == pages->end())
            return nullptr;
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html;charset=utf-8");
          response->set_content(it->second);
          return response;
        },
        base::Unretained(&pages_)));
    ASSERT_TRUE(https_server_.InitializeAndListen());
    https_server_.StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    // Make sure to close any showing popups prior to tearing down the UI.
    ContentAutofillDriver::GetForRenderFrameHost(main_frame())
        ->autofill_manager()
        ->client()
        ->HideAutofillPopup(PopupHidingReason::kTabGone);
    test::ReenableSystemServices();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    cert_verifier_.SetUpCommandLine(command_line);
    // Slower test bots (ChromeOS, debug, etc.) are flaky due to slower loading
    // interacting with deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  // Registers the response `content_html` for a given `relative_path`, wth
  // all placeholders $1, $2, ... in `content_html` replaced with the
  // corresponding hostname from `kHostnames`.
  // This response is served by for *every* hostname.
  void SetUrlContent(std::string relative_path,
                     base::StringPiece content_html) {
    ASSERT_EQ(relative_path[0], '/');
    std::vector<std::string> replacements;
    replacements.reserve(std::size(kHostnames));
    for (const char* hostname : kHostnames) {
      replacements.push_back(std::string(
          base::TrimString(https_server_.GetURL(hostname, "/").spec(), "/",
                           base::TRIM_TRAILING)));
    }
    pages_[std::move(relative_path)] =
        base::ReplaceStringPlaceholders(content_html, replacements, nullptr);
  }

  // Navigates on https://`kMainHostname`:some_port/`relative_url` and returns a
  // form, if one exists, that has `num_fields` fields.
  //
  // If `click_to_reparse`, it additionally clicks into the first field of each
  // frame (if such a field exists). See GetOrWaitForFormWithFocusableFields()
  // for details why.
  //
  // Each test shall prepare the intended response using SetUrlContent() in
  // advance.
  const FormStructure* NavigateToUrl(base::StringPiece relative_url,
                                     size_t num_fields,
                                     bool click_to_reparse = false) {
    NavigateParams params(browser(),
                          https_server_.GetURL(kMainHostname, relative_url),
                          ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
    return GetOrWaitForFormWithFocusableFields(
        /*num_fields=*/num_fields, /*click_to_reparse=*/click_to_reparse);
  }

  // Returns a form with `num_fields` fields. If no such form exists and no such
  // form appears within a timeout, returns nullptr. If `click_to_reparse` is
  // true, simulates a click in one field of each frame, which causes
  // re-extraction of the form.
  //
  // The reason for the click is a (rather rare) race condition when receiving
  // frames: when FormForest
  // (a) receives a child form before the parent form, and
  // (b) the received parent form doesn't contain to the child form's
  //     FrameToken (but instead an outdated FrameToken),
  // then no reparse is triggered. This is because FormForest hasn't seen the
  // parent frame yet and therefore doesn't know its ContentAutofillDriver.
  // We could fix this using ContentAutofillDriver::GetForRenderFrameHost()
  // in FormForest::UpdateTreeOfRendererForm().
  //
  // Clicking into field of the iframe re-extracts the iframe's form and sends
  // it to the browser. If necessary, FormForest would then trigger a reparse in
  // the parent frame. We choose this workaround rather than triggering reparses
  // more directly (by calling ContentAutofillDriver::TriggerReparse()) because
  // clicking is typical real-world behaviour.
  //
  // Another, unrelated issue is focusability: sometimes fields are unfocusable
  // (FormFieldData::is_focusable is false) when they are extracted on page
  // load. This issue appears to be unrelated to AutofillAcrossIframes, it's
  // probably just a race condition between Blink and Autofill's form
  // extraction. Focusing a field re-extracts the field's form, and then fields
  // seem to be focusable. That is, clicking into some field of each form in
  // each frame would likely work around the focusability issue for the purposes
  // of this bug. However, since clicking into each may also have other side
  // effects (parsing more forms again) and is not common user behaviour, we do
  // not simulate such clicks. Instead, we simply override
  // FormFieldData::is_focusable for all forms. This is admissible for our
  // testing purposes because all test forms only have (what should be)
  // focusable fields.
  // TODO(crbug.com/1393058): Remove this hack when the focusability issue is
  // fixed.
  const FormStructure* GetOrWaitForFormWithFocusableFields(
      size_t num_fields,
      bool click_to_reparse) {
    if (click_to_reparse) {
      size_t num_clicked = 0;
      main_frame()->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
        num_clicked += ClickFirstField(web_contents(), rfh);
      });
      EXPECT_GT(num_clicked, 0u);
    }
    const FormStructure* form =
        main_autofill_manager()->WaitForMatchingForm(base::BindRepeating(
            [](size_t num_fields, const FormStructure& form) {
              return num_fields == form.field_count();
            },
            num_fields));
    for (const auto& field : *form)
      const_cast<AutofillField&>(*field).is_focusable = true;
    return form;
  }

  // Fills the form in the DOM that corresponds to `form_structure` and returns
  // the filled values. The order of the values is aligned with the order of the
  // `form_structure` fields.
  std::vector<std::string> FillForm(const FormStructure& form_structure,
                                    const AutofillField& trigger_field) {
    const FormData& form = form_structure.ToFormData();
    FillCard(main_frame(), form, trigger_field);
    return AllFieldValues(web_contents(), form);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestAutofillManager* main_autofill_manager() {
    return TestAutofillManager::GetForRenderFrameHost(main_frame());
  }

 private:
  static constexpr std::array kHostnames = {"a.com", "b.com", "c.com",
                                            "d.com", "e.com", "f.com"};
  static constexpr const char* kMainHostname = kHostnames[0];

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
  content::ContentMockCertVerifier cert_verifier_;
  // Maps relative paths to HTML content.
  std::map<std::string, std::string> pages_;
  TestAutofillManagerFutureInjectors<TestAutofillManager> injectors_;
};

// Test fixture for basic filling, in particular for testing the security policy
// (same-origin policy and shared-autofill).
class AutofillAcrossIframesTest_Simple : public AutofillAcrossIframesTest {
 public:
  // Creates a simple form
  //   <form>
  //     <iframe><input autocomplete=cc-name></iframe>
  //     <iframe><input autocomplete=cc-number></iframe>
  //     <iframe><input autocomplete=cc-exp></iframe>
  //     <iframe><input autocomplete=cc-csc></iframe>
  //   </form>
  // where the hostnames and attributes, such as "allow=shared-autofill" or
  // "sandbox", can be configured.
  [[nodiscard]] const FormStructure* LoadForm(
      std::array<const char*, 4> hostnames = {"$1", "$1", "$1", "$1"},
      std::array<const char*, 4> attributes = {"", "", "", ""}) {
    SetUrlContent("/name.html", R"(<input autocomplete=cc-name>)");
    SetUrlContent("/num.html", R"(<input autocomplete=cc-number>)");
    SetUrlContent("/exp.html", R"(<input autocomplete=cc-exp>)");
    SetUrlContent("/cvc.html", R"(<input autocomplete=cc-csc>)");
    SetUrlContent(
        "/", base::StringPrintf(
                 R"(<iframe %s src="%s/name.html"></iframe>
                    <iframe %s src="%s/num.html"></iframe>
                    <iframe %s src="%s/exp.html"></iframe>
                    <iframe %s src="%s/cvc.html"></iframe>)",
                 attributes[0], hostnames[0], attributes[1], hostnames[1],
                 attributes[2], hostnames[2], attributes[3], hostnames[3]));
    return NavigateToUrl("/", /*num_fields=*/4);
  }
};

// Tests that autofilling on a main-origin field fills all same-origin fields.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_Simple, SameOrigin_FillAll) {
  const FormStructure* form = LoadForm({"$1", "$1", "$1", "$1"});
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(0)),
              ElementsAre(kNameFull, kNumber, kExp, kCvc));
}

// Tests that autofilling on a main-origin field fills only fills on the main
// origin.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_Simple, CrossOrigin_FillName) {
  const FormStructure* form = LoadForm({"$1", "$2", "$3", "$4"});
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(0)),
              ElementsAre(kNameFull, "", "", ""));
}

// Tests that autofilling on a cross-origin field fills only fills on that
// origin and on the main origin (if it's a non-sensitive field).
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_Simple,
                       CrossOrigin_FillNameAndNumber) {
  const FormStructure* form = LoadForm({"$1", "$2", "$3", "$4"});
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre(kNameFull, kNumber, "", ""));
}

// Tests that autofilling on a cross-origin field fills only fills on that
// origin and on the main origin only non-sensitive fields.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_Simple,
                       CrossOrigin_FillNameOnMainOrigin) {
  const FormStructure* form = LoadForm({"$1", "$2", "$1", "$1"});
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre(kNameFull, kNumber, "", ""));
}

// Tests that sandboxed frames are treated like other cross-origin frames.
//
// This test seemed flaky in one patchset due to a DCHECK in
// content_settings::PatternPair GetPatternsFromScopingType(), but the issue
// didn't occur afterwards.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_Simple,
                       Sandboxed_FillOnlyNumber) {
  // Our test fixture needs allow-scripts to extract the field values.
  static constexpr char sandbox[] = "sandbox=allow-scripts";
  const FormStructure* form =
      LoadForm({"$1", "$1", "$1", "$1"}, {sandbox, sandbox, sandbox, sandbox});
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre("", kNumber, "", ""));
}

// Test fixture for "shared-autofill". The parameter indicates whether or not
// shared-autofill has the "relaxed" semantics.
class AutofillAcrossIframesTest_SharedAutofill
    : public AutofillAcrossIframesTest_Simple,
      public ::testing::WithParamInterface<bool> {
 public:
  AutofillAcrossIframesTest_SharedAutofill() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillSharedAutofill,
        {{features::kAutofillSharedAutofillRelaxedParam.name,
          is_relaxed() ? "true" : "false"}});
  }

  bool is_relaxed() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AutofillAcrossIframesTest,
                         AutofillAcrossIframesTest_SharedAutofill,
                         ::testing::Bool());

// Tests that autofilling on a main-origin field also fills cross-origin fields
// whose frames have shared-autofill enabled.
IN_PROC_BROWSER_TEST_P(AutofillAcrossIframesTest_SharedAutofill,
                       FillWhenTriggeredOnMainOrigin) {
  const FormStructure* form =
      LoadForm({"$1", "$2", "$3", "$4"}, {"", "", "", "allow=shared-autofill"});
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(0)),
              ElementsAre(kNameFull, "", "", kCvc));
}

// Tests that autofilling on a cross-origin field also fills cross-origin fields
// whose frames have shared-autofill enabled iff shared-autofill is relaxed.
IN_PROC_BROWSER_TEST_P(AutofillAcrossIframesTest_SharedAutofill,
                       FillWhenTriggeredOnNonMainOriginIffRelaxed) {
  const FormStructure* form =
      LoadForm({"$1", "$2", "$3", "$4"}, {"", "", "", "allow=shared-autofill"});
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre(kNameFull, kNumber, "", is_relaxed() ? kCvc : ""));
}

// Tests that autofilling on a cross-origin field also fills main-origin fields
// irrespective of their type if shared-autofill is relaxed.
IN_PROC_BROWSER_TEST_P(
    AutofillAcrossIframesTest_SharedAutofill,
    FillEverythingOnMainOriginWhenTriggeredOnNonMainOriginIffRelaxed) {
  const FormStructure* form =
      LoadForm({"$1", "$2", "$1", "$1"}, {"", "", "", "allow=shared-autofill"});
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre(kNameFull, kNumber, is_relaxed() ? kExp : "",
                          is_relaxed() ? kCvc : ""));
}

// Test fixture where a form changes dynamically when it is filled.
class AutofillAcrossIframesTest_Dynamic : public AutofillAcrossIframesTest {
 public:
  // Adds the CVC iframe (including a field) dynamically when the rest of the
  // form is filled.
  const FormStructure* LoadFormWithAppearingFrame() {
    SetUrlContent("/name.html", R"(
      <input autocomplete=cc-name>
      <script>
        document.body.firstElementChild.onchange = function() {
          const doc = window.parent.document;
          const iframe = doc.createElement('iframe');
          iframe.src = '$1/cvc.html';
          doc.body.appendChild(iframe);
        };
      </script> )");
    SetUrlContent("/num.html", R"(<input autocomplete=cc-number>)");
    SetUrlContent("/exp.html", R"(<input autocomplete=cc-exp>)");
    SetUrlContent("/cvc.html", R"(<input autocomplete=cc-csc>)");
    SetUrlContent("/", base::StringPrintf(
                           R"(<iframe src="$1/name.html"></iframe>
                              <iframe src="$1/num.html"></iframe>
                              <iframe src="$1/exp.html"></iframe>)"));
    return NavigateToUrl("/", /*num_fields=*/3);
  }

  // Adds the CVC field dynamically when the rest of the form is filled.
  const FormStructure* LoadFormWithAppearingField() {
    SetUrlContent("/name.html", R"(
      <input autocomplete=cc-name>
      <script>
        document.body.firstElementChild.onchange = function() {
          const doc = window.parent.frames[3].document;
          const input = doc.createElement('input');
          input.autocomplete = 'cc-csc';
          doc.body.appendChild(input);
        };
      </script> )");
    SetUrlContent("/num.html", R"(<input autocomplete=cc-number>)");
    SetUrlContent("/exp.html", R"(<input autocomplete=cc-exp>)");
    SetUrlContent("/cvc.html", "");
    SetUrlContent("/", base::StringPrintf(
                           R"(<iframe src="$1/name.html"></iframe>
                              <iframe src="$1/num.html"></iframe>
                              <iframe src="$1/exp.html"></iframe>
                              <iframe src="$1/cvc.html"></iframe>)"));
    return NavigateToUrl("/", /*num_fields=*/3);
  }

  // Fills `form_structure` and returns the filled values. The order of the
  // values is aligned with the order of the `form_structure` fields.
  std::vector<std::string> FillForm(const FormStructure& form_structure,
                                    const AutofillField& trigger_field) {
    FormData form = form_structure.ToFormData();
    EXPECT_EQ(3u, form.fields.size());  // The CVC field doesn't exist yet.
    TestAutofillManager* manager = main_autofill_manager();
    FillCard(main_frame(), form, trigger_field);
    // Now, after FillCard(), the form gets filled in the renderer (which
    // triggers three OnDidFillAutofillFormData() events) and then changes.
    // The change triggers an OnFormsSeen() event, followed by a re-parse and
    // re-fill, which then triggers another four OnDidFillAutofillFormData()
    // events.
    EXPECT_TRUE(manager->WaitForAutofill(3 + 4));
    form =
        manager->form_structures().find(form.global_id())->second->ToFormData();
    EXPECT_EQ(4u, form.fields.size());  // The CVC field has now been seen.
    return AllFieldValues(web_contents(), form);
  }
};

// Tests that a newly emerging frame with a field triggers a refill.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_Dynamic,
                       RefillDynamicFormWithNewFrame) {
  const FormStructure* form = LoadFormWithAppearingFrame();
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre(kNameFull, kNumber, kExp, kCvc));
}

// Tests that a newly emerging field inside a frame triggers a refill.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_Dynamic,
                       RefillDynamicFormWithNewField) {
  const FormStructure* form = LoadFormWithAppearingField();
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre(kNameFull, kNumber, kExp, kCvc));
}

// Test fixture that removes a frame right before the fill. This shall not
// confuse the form filling, in particular, it shall not crash.
class AutofillAcrossIframesTest_DeletedFrame
    : public AutofillAcrossIframesTest_Simple {
 public:
  std::vector<std::string> FillForm(const FormStructure& form_structure,
                                    const AutofillField& trigger_field) {
    FormData form = form_structure.ToFormData();
    EXPECT_EQ(4u, form.fields.size());
    EXPECT_EQ(5u, num_frames());
    std::ignore = content::EvalJs(
        main_frame(),
        R"( document.getElementsByTagName('iframe')[1].remove(); )");
    EXPECT_EQ(4u, num_frames());
    FillCard(main_frame(), form, trigger_field);
    form.fields.erase(form.fields.begin() + 1);
    return AllFieldValues(web_contents(), form);
  }

 private:
  // Returns the number of frames in the frame tree, including the main frame.
  size_t num_frames() {
    size_t num = 0;
    main_frame()->ForEachRenderFrameHost(
        [&num](content::RenderFrameHost* rfh) { ++num; });
    return num;
  }
};

// Tests that we don't crash if the filling data is referring to a non-existent
// frame.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_DeletedFrame,
                       DeletingFrameDuringFillDoesNotCrash) {
  const FormStructure* form = LoadForm();
  ASSERT_TRUE(form);
  EXPECT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre(kNameFull, kExp, kCvc));
}

// Test fixture for huge forms.
class AutofillAcrossIframesTest_NestedAndLargeForm
    : public AutofillAcrossIframesTest {
 public:
  // Manually specify large frame sizes. This simplifies clicking into them.
  std::string MakeCss(size_t height) {
    return base::StringPrintf(
        R"(<style>
           * { margin: 0; padding: 0; }
           iframe { height: %zupx; width: %zupx; border: 0; }
           input { display: block; height: 20px; width: 100px; }
           </style>)",
        height * 100, 100 + height * 10);
  }
};

// Tests that a large and deeply nested form is extracted and filled correctly.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_NestedAndLargeForm,
                       FillAllFieldsOnTriggeredOrigin) {
  SetUrlContent("/", MakeCss(3) +
                         R"(<iframe src="$4/3.html"></iframe>
                            <iframe src="$3/3.html"></iframe>
                            <iframe src="$2/3.html"></iframe>
                            <iframe src="$1/3.html"></iframe>)");
  SetUrlContent("/3.html", MakeCss(2) +
                               R"(<form>
                                  <input autocomplete=cc-name>
                                  <input>
                                  <iframe src="$2/2.html"></iframe>
                                  <input>
                                  <input autocomplete=cc-csc>
                                  </form>)");
  SetUrlContent("/2.html", MakeCss(1) +
                               R"(<form>
                                  <input autocomplete=cc-number>
                                  <input>
                                  <iframe src="$1/1.html"></iframe>
                                  <input>
                                  <input autocomplete=cc-exp>
                                  </form>)");
  SetUrlContent("/1.html", MakeCss(0) +
                               R"(<form>
                                  <input autocomplete=cc-name>
                                  <input autocomplete=cc-number>
                                  <input autocomplete=cc-exp>
                                  <input autocomplete=cc-csc>
                                  </form>)");
  const FormStructure* form =
      NavigateToUrl("/", /*num_fields=*/48, /*click_to_reparse=*/true);
  ASSERT_TRUE(form);
  ASSERT_THAT(*form, IsWithinAutofillLimits());
  {
    // Test that the parsed form reflects the structure of the above <iframe>
    // and <form> elements.
    auto name = HtmlFieldType::kCreditCardNameFull;
    auto num = HtmlFieldType::kCreditCardNumber;
    auto exp = HtmlFieldType::kCreditCardExp;
    auto cvc = HtmlFieldType::kCreditCardVerificationCode;
    auto unspecified = HtmlFieldType::kUnspecified;
    auto m = [](base::StringPiece host, HtmlFieldType type) {
      return Pointee(AllOf(Property(&AutofillField::html_type, Eq(type)),
                           Field(&AutofillField::origin,
                                 Property(&url::Origin::host, Eq(host)))));
    };
    // The indentation reflects the nesting of frames.
    // clang-format off
    EXPECT_THAT(form->fields(),
                ElementsAre(
                    // $3/3.html
                    m("d.com", name),
                    m("d.com", unspecified),
                      m("b.com", num),
                      m("b.com", unspecified),
                        m("a.com", name),
                        m("a.com", num),
                        m("a.com", exp),
                        m("a.com", cvc),
                      m("b.com", unspecified),
                      m("b.com", exp),
                    m("d.com", unspecified),
                    m("d.com", cvc),
                    // $4/3.html
                    m("c.com", name),
                    m("c.com", unspecified),
                      m("b.com", num),
                      m("b.com", unspecified),
                        m("a.com", name),
                        m("a.com", num),
                        m("a.com", exp),
                        m("a.com", cvc),
                      m("b.com", unspecified),
                      m("b.com", exp),
                    m("c.com", unspecified),
                    m("c.com", cvc),
                    // $5/3.html
                    m("b.com", name),
                    m("b.com", unspecified),
                      m("b.com", num),
                      m("b.com", unspecified),
                        m("a.com", name),
                        m("a.com", num),
                        m("a.com", exp),
                        m("a.com", cvc),
                      m("b.com", unspecified),
                      m("b.com", exp),
                    m("b.com", unspecified),
                    m("b.com", cvc),
                    // $6/3.html
                    m("a.com", name),
                    m("a.com", unspecified),
                      m("b.com", num),
                      m("b.com", unspecified),
                        m("a.com", name),
                        m("a.com", num),
                        m("a.com", exp),
                        m("a.com", cvc),
                      m("b.com", unspecified),
                      m("b.com", exp),
                    m("a.com", unspecified),
                    m("a.com", cvc)
                  ));
    // clang-format on
  }
  const FormData& form_data = form->ToFormData();
  ASSERT_EQ("a.com", form_data.fields[4].origin.host());
  ASSERT_EQ("cc-name", form_data.fields[4].autocomplete_attribute);
  FillCard(main_frame(), form_data, form_data.fields[4]);
  EXPECT_TRUE(main_autofill_manager()->WaitForAutofill(5));
  {
    const auto* name = kNameFull;
    const auto* num = kNumber;
    const auto* exp = kExp;
    const auto* cvc = kCvc;
    std::vector<std::string> values = AllFieldValues(web_contents(), form_data);
    EXPECT_THAT(
        values,
        ElementsAre("", "", "", "", name, num, exp, cvc, "", "", "", "",  //
                    "", "", "", "", name, num, exp, cvc, "", "", "", "",  //
                    "", "", "", "", name, num, exp, cvc, "", "", "", "",  //
                    name, "", "", "", name, num, exp, cvc, "", "", "", cvc));
  }
}

// Tests that a deeply nested form where some iframes don't even contain any
// fields (but their subframes do) is extracted and filled correctly.
IN_PROC_BROWSER_TEST_F(AutofillAcrossIframesTest_NestedAndLargeForm,
                       // TODO(crbug.com/1393990): Re-enable this test
                       DISABLED_FlattenFormEvenAcrossFramesWithoutFields) {
  SetUrlContent("/", MakeCss(3) +
                         R"(<iframe src="$4/3.html"></iframe>
                            <iframe src="$3/3.html"></iframe>
                            <iframe src="$2/3.html"></iframe>
                            <iframe src="$1/3.html"></iframe>)");
  SetUrlContent(
      "/3.html",
      MakeCss(2) + R"(<form><iframe src="$2/2.html"></iframe></form>)");
  SetUrlContent("/2.html", MakeCss(1) + R"(<iframe src="$1/1.html"></iframe>)");
  SetUrlContent("/1.html", MakeCss(0) +
                               R"(<form><input autocomplete=cc-name></form>
                                  <form><input autocomplete=cc-number></form>
                                  <form><input autocomplete=cc-exp></form>
                                  <form><input autocomplete=cc-csc></form>)");
  const FormStructure* form =
      NavigateToUrl("/", /*num_fields=*/16, /*click_to_reparse=*/true);
  ASSERT_TRUE(form);
  ASSERT_THAT(*form, IsWithinAutofillLimits());
  {
    // Test that the parsed form reflects the structure of the above <iframe>
    // and <form> elements.
    auto name = HtmlFieldType::kCreditCardNameFull;
    auto num = HtmlFieldType::kCreditCardNumber;
    auto exp = HtmlFieldType::kCreditCardExp;
    auto cvc = HtmlFieldType::kCreditCardVerificationCode;
    auto m = [](HtmlFieldType type) {
      return Pointee(AllOf(Property(&AutofillField::html_type, Eq(type)),
                           Field(&AutofillField::origin,
                                 Property(&url::Origin::host, Eq("a.com")))));
    };
    EXPECT_THAT(form->fields(), ElementsAre(m(name), m(num), m(exp), m(cvc),  //
                                            m(name), m(num), m(exp), m(cvc),  //
                                            m(name), m(num), m(exp), m(cvc),  //
                                            m(name), m(num), m(exp), m(cvc)));
  }
  const FormData& form_data = form->ToFormData();
  FillCard(main_frame(), form_data, form_data.fields[0]);
  EXPECT_TRUE(main_autofill_manager()->WaitForAutofill(4));
  {
    const auto* name = kNameFull;
    const auto* num = kNumber;
    const auto* exp = kExp;
    const auto* cvc = kCvc;
    std::vector<std::string> values = AllFieldValues(web_contents(), form_data);
    EXPECT_THAT(values, ElementsAre(name, num, exp, cvc, name, num, exp, cvc,
                                    name, num, exp, cvc, name, num, exp, cvc));
  }
}

// Test fixture for detecting form submission. The parameter indicates whether
// the submission occurs in the main frame or an iframe.
class AutofillAcrossIframesTest_Submission
    : public AutofillAcrossIframesTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool submission_happens_in_main_frame() const { return GetParam(); }

  // Creates a simple cross-frame form with <form> elements so we can submit the
  // form in the iframe and the main frame.
  //
  // Just to mix things up a little compared to the other tests, here the
  // "name" field is in the main frame, not just on the main frame origin.
  [[nodiscard]] const FormStructure* LoadForm(
      std::array<const char*, 3> hostnames = {"$1", "$1", "$1"}) {
    auto frame_html = [](const char* autocomplete) {
      return base::StringPrintf(R"(<form action="$1/submit.html" method="GET">
                                      <input name="%s" autocomplete="%s">
                                    </form>)",
                                autocomplete, autocomplete);
    };
    SetUrlContent("/num.html", frame_html("cc-number"));
    SetUrlContent("/exp.html", frame_html("cc-exp"));
    SetUrlContent("/cvc.html", frame_html("cc-csc"));
    SetUrlContent("/submit.html", "<h1>Submitted</h1>");
    SetUrlContent("/", base::StringPrintf(
                           R"(<form method=GET action=submit.html>
                              <input name=cc-name autocomplete=cc-name>
                              <iframe src="%s/num.html"></iframe>
                              <iframe src="%s/exp.html"></iframe>
                              <iframe src="%s/cvc.html"></iframe>
                              </form>)",
                           hostnames[0], hostnames[1], hostnames[2]));
    return NavigateToUrl("/", /*num_fields=*/4);
  }

  [[nodiscard]] AssertionResult SubmitInIframe() {
    bool submitted = false;
    AssertionResult result = AssertionFailure() << "No frame found";
    main_frame()->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
      if (!rfh->IsInPrimaryMainFrame() && !submitted) {
        result = SubmitInFrame(rfh);
        submitted = true;
      }
    });
    return result ? main_autofill_manager()->WaitForSubmission(1) : result;
  }

  [[nodiscard]] AssertionResult SubmitInMainFrame() {
    AssertionResult result = SubmitInFrame(main_frame());
    return result ? main_autofill_manager()->WaitForSubmission(1) : result;
  }

  [[nodiscard]] AssertionResult SubmitInFrame(content::RenderFrameHost* rfh) {
    return content::ExecJs(rfh, R"(document.forms[0].submit();)");
  }
};

INSTANTIATE_TEST_SUITE_P(AutofillAcrossIframesTest,
                         AutofillAcrossIframesTest_Submission,
                         ::testing::Bool());

// Tests that submission of a cross-frame form is detected in the main frame.
IN_PROC_BROWSER_TEST_P(AutofillAcrossIframesTest_Submission,
                       SubmissionGetsDetected) {
  const FormStructure* form = LoadForm({"$2", "$2", "$2"});
  ASSERT_TRUE(form);
  ASSERT_THAT(FillForm(*form, *form->field(1)),
              ElementsAre(kNameFull, kNumber, kExp, kCvc));
  ASSERT_TRUE(submission_happens_in_main_frame() ? SubmitInMainFrame()
                                                 : SubmitInIframe());
  EXPECT_THAT(main_autofill_manager()->submitted_form(),
              Optional(Field(&FormData::fields,
                             ElementsAre(HasValue(kNameFull), HasValue(kNumber),
                                         HasValue(kExp), HasValue(kCvc)))));
}

}  // namespace autofill
