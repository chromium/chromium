// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/autofill_flow_test_util.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_autofill_tick_clock.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/dom_us_layout_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(ENABLE_EXTENSIONS)
// Includes for ChromeVox accessibility tests.
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "extensions/browser/browsertest_util.h"
#include "ui/base/test/ui_controls.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(ENABLE_EXTENSIONS)

using base::ASCIIToUTF16;
using content::URLLoaderInterceptor;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAreArray;

namespace autofill {

namespace {

constexpr char kTestShippingFormString[] = R"(
  <html>
  <head>
    <!-- Disable extra network request for /favicon.ico -->
    <link rel="icon" href="data:,">
  </head>
  <body>
    An example of a shipping address form.
    <form action="https://www.example.com/" method="POST" id="shipping">
    <label for="firstname">First name:</label>
     <input type="text" id="firstname"><br>
    <label for="lastname">Last name:</label>
     <input type="text" id="lastname"><br>
    <label for="address1">Address line 1:</label>
     <input type="text" id="address1"><br>
    <label for="address2">Address line 2:</label>
     <input type="text" id="address2"><br>
    <label for="city">City:</label>
     <input type="text" id="city"><br>
    <label for="state">State:</label>
     <select id="state">
     <option value="" selected="yes">--</option>
     <option value="CA">California</option>
     <option value="TX">Texas</option>
     </select><br>
    <label for="zip">ZIP code:</label>
     <input type="text" id="zip"><br>
    <label for="country">Country:</label>
     <select id="country">
     <option value="" selected="yes">--</option>
     <option value="CA">Canada</option>
     <option value="US">United States</option>
     </select><br>
    <label for="phone">Phone number:</label>
     <input type="text" id="phone"><br>
    </form>
    )";

constexpr std::string_view kNumElementsMatchesNumFields =
    "Autofill.NumElementsMatchesNumFields";

ACTION_P(InvokeClosure, closure) {
  closure.Run();
}

// Continuously merges histograms from all subprocesses and checks if the
// histogram `histogram_name` got observed with `expected_count` and
// `expected_sample`. Then runs `base::HistogramTester::ExpectUniqueSample`.
bool WaitAndExpectUniqueSample(const base::HistogramTester* histogram_tester,
                               const std::string_view histogram_name,
                               const bool expected_sample,
                               const int expected_count) {
  bool expected_count_observed = base::test::RunUntil([&]() {
    ::metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    return histogram_tester->GetBucketCount(histogram_name, expected_sample) ==
           expected_count;
  });
  histogram_tester->ExpectUniqueSample(kNumElementsMatchesNumFields,
                                       expected_sample, expected_count);
  return expected_count_observed;
}

// Version of `kTestShippingFormString` which uses <selectlist> instead of
// <select>.
std::string GenerateTestShippingFormWithSelectList() {
  std::string out = kTestShippingFormString;
  RE2::GlobalReplace(&out, "<(/?)select", "<\\1selectlist");
  return out;
}

// Searches all frames of the primary page in |web_contents| and returns one
// called |name|. If there are none, returns null, if there are more, returns
// an arbitrary one.
content::RenderFrameHost* RenderFrameHostForName(
    content::WebContents* web_contents,
    const std::string& name) {
  return content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, name));
}

autofill::ElementExpr GetElementById(const std::string& id) {
  return autofill::ElementExpr(
      base::StringPrintf("document.getElementById(`%s`)", id.c_str()));
}

// Represents a field's expected or actual (as extracted from the DOM) id (not
// its name) and value.
struct FieldValue {
  std::string id;
  std::string value;
};

std::ostream& operator<<(std::ostream& os, const FieldValue& field) {
  return os << "{" << field.id << "=" << field.value << "}";
}

// Returns the field IDs and values of a collection of fields.
//
// Note that `control_elements` is *not* the ID of a form, but a JavaScript
// expression that evaluates to a collection of form-control elements, such has
// `document.getElementById('myForm').elements`.
std::vector<FieldValue> GetFieldValues(
    const ElementExpr& control_elements,
    content::ToRenderFrameHost execution_target) {
  std::string script = base::StringPrintf(
      R"( const fields = [];
          for (const field of %s) {
            fields.push({
              id: field.id,
              value: field.value
            });
          }
          fields;
        )",
      control_elements->c_str());
  content::EvalJsResult r = content::EvalJs(execution_target, script);
  DCHECK(r.value.is_list()) << r.error;
  std::vector<FieldValue> fields;

  for (const base::Value& field : r.value.GetList()) {
    const auto& field_dict = field.GetDict();
    fields.push_back({.id = *field_dict.FindString("id"),
                      .value = *field_dict.FindString("value")});
  }
  return fields;
}

// Types the characters of `value` after focusing field `e`.
[[nodiscard]] AssertionResult EnterTextIntoField(
    const autofill::ElementExpr& e,
    base::StringPiece value,
    AutofillUiTest* test,
    content::ToRenderFrameHost execution_target) {
  AssertionResult a = FocusField(e, execution_target);
  if (!a) {
    return a;
  }

  for (const char c : value) {
    ui::DomKey key = ui::DomKey::FromCharacter(c);
    if (!test->SendKeyToPageAndWait(key, {})) {
      return AssertionFailure()
             << __func__ << "(): Could not type '" << value << "' into " << *e;
    }
  }

  return AssertionSuccess();
}

const std::vector<FieldValue> kEmptyAddress{
    {"firstname", ""}, {"lastname", ""}, {"address1", ""},
    {"address2", ""},  {"city", ""},     {"state", ""},
    {"zip", ""},       {"country", ""},  {"phone", ""}};

const struct {
  const char* first_name = "Milton";
  const char* middle_name = "C.";
  const char* last_name = "Waddams";
  const char* full_name = "Milton C. Waddams";
  const char* address1 = "4120 Freidrich Lane";
  const char* address2 = "Basement";
  const char* city = "Austin";
  const char* state_short = "TX";
  const char* state = "Texas";
  const char* zip = "78744";
  const char* country = "US";
  const char* phone = "5125551234";
  const char* company = "Initech";
  const char* email = "red.swingline@initech.com";
} kDefaultAddressValues;

const std::vector<FieldValue> kDefaultAddress{
    {"firstname", kDefaultAddressValues.first_name},
    {"lastname", kDefaultAddressValues.last_name},
    {"address1", kDefaultAddressValues.address1},
    {"address2", kDefaultAddressValues.address2},
    {"city", kDefaultAddressValues.city},
    {"state", kDefaultAddressValues.state_short},
    {"zip", kDefaultAddressValues.zip},
    {"country", kDefaultAddressValues.country},
    {"phone", kDefaultAddressValues.phone}};

// Returns a copy of |fields| except that the value of `update.id` is set to
// `update.value`.
[[nodiscard]] std::vector<FieldValue> MergeValue(std::vector<FieldValue> fields,
                                                 const FieldValue& update) {
  for (auto& field : fields) {
    if (field.id == update.id) {
      field.value = update.value;
      return fields;
    }
  }
  NOTREACHED();
  return fields;
}

// Returns a copy of |fields| merged with |updates|.
[[nodiscard]] std::vector<FieldValue> MergeValues(
    std::vector<FieldValue> fields,
    const std::vector<FieldValue>& updates) {
  for (auto& update : updates)
    fields = MergeValue(std::move(fields), update);
  return fields;
}

// A generic "map" function, intended to lift values `args...` to a matcher
// `fun(args)...`. For example, `ElementsAreArray(Map({x, y, z}, fun))` is
// `ElementsAreArray({fun(x), fun(y), fun(z)})`.
template <typename Arg, typename Fun>
[[nodiscard]] auto Map(const std::vector<Arg>& args, Fun fun) {
  std::vector<decltype(std::invoke(fun, args[0]))> matchers;
  for (const Arg& arg : args) {
    matchers.push_back(std::invoke(fun, arg));
  }
  return matchers;
}

// Matches a container of FieldValues if the `i`th actual FieldValue::value
// matches the `i`th `expected` FieldValue::value.
// As a sanity check, also requires that the `i`th actual FieldValue::id
// starts with the `i`th `expected` FieldValue::id.
[[nodiscard]] auto ValuesAre(const std::vector<FieldValue>& expected) {
  return UnorderedElementsAreArray(
      Map(expected, [](const FieldValue& expected) {
        return AllOf(Field(&FieldValue::id, StartsWith(expected.id)),
                     Field(&FieldValue::value, Eq(expected.value)));
      }));
}

[[nodiscard]] auto FieldsAre(auto matcher) {
  return Field(&FormData::fields, ElementsAreArray(matcher));
}

// An object that waits for an observed form-control element to change its value
// to a non-empty string.
//
// See ListenForValueChange() for details.
class ValueWaiter {
 public:
  static constexpr base::TimeDelta kDefaultTimeout = base::Seconds(5);

  ValueWaiter(int waiterId, content::ToRenderFrameHost execution_target)
      : waiterId_(waiterId), execution_target_(execution_target) {}

  // Returns the non-empty value of the observed form-control element, or
  // absl::nullopt if no value change is observed before `timeout`.
  [[nodiscard]] absl::optional<std::string> Wait(
      base::TimeDelta timeout = kDefaultTimeout) && {
    const std::string kFunction = R"(
      // Polls the value of `window[observedValueSlots]` and replies with the
      // value once its non-`undefined` or `timeoutMillis` have elapsed.
      //
      // The value is expected to be populated by listenForValueChange().
      function pollValue(waiterId, timeoutMillis) {
        console.log(`pollValue('${waiterId}', ${timeoutMillis})`);

        let interval = undefined;
        let timeout = undefined;

        return new Promise(resolve => {
          function reply(r) {
            console.log(`pollValue('${waiterId}', ${timeoutMillis}): `+
                        `replying '${r}'`);
            resolve(r);
            clearTimeout(timeout);
            clearInterval(interval);
          }

          function replyIfSet(r) {
            if (r !== undefined)
              reply(r);
          }

          timeout = setTimeout(function() {
            console.log(`pollValue('${waiterId}', ${timeoutMillis}): timeout`);
            reply(null);
          }, timeoutMillis);

          const kPollingIntervalMillis = 100;
          interval = setInterval(function() {
            replyIfSet(window.observedValueSlots[waiterId]);
          }, kPollingIntervalMillis);

          replyIfSet(window.observedValueSlots[waiterId]);
        });
      }
    )";
    std::string call = base::StringPrintf("pollValue(`%d`, %" PRId64 ")",
                                          waiterId_, timeout.InMilliseconds());
    content::EvalJsResult r =
        content::EvalJs(execution_target_, kFunction + call);
    return !r.value.is_none() ? absl::make_optional(r.ExtractString())
                              : absl::nullopt;
  }

 private:
  int waiterId_;
  content::ToRenderFrameHost execution_target_;
};

// Registers observers for a value change of a field `id`. This listener fires
// on the first time *any* object whose ID is `id` changes its value to a
// non-empty string after the global `unblock_variable` has become true.
//
// It is particularly useful for detecting refills.
//
// For example, consider the following chain JavaScript statements:
//
// 1. window.unblock = undefined // or any other value that converts to false;
// 2. document.body.innerHTML += '<input id="id">';
// 3. document.getElementById('id').value = "foo";
// 4. document.getElementById('id').remove();
// 5. document.body.innerHTML += '<input id="id">';
// 6. document.getElementById('id').value = "foo";
// 7. window.unblock = true;
// 8. document.getElementById('id').value = "";
// 9. document.getElementById('id').value = "bar";
//
// Then `ListenForValueChange("id", "unblock", rfh).Wait(base::Seconds(5)) ==
// "bar"`. The ListenForValueChange() call happens any point before Event 9, and
// Event 9 happens no later than 5 seconds after that.
[[nodiscard]] ValueWaiter ListenForValueChange(
    const std::string& id,
    const absl::optional<std::string>& unblock_variable,
    content::ToRenderFrameHost execution_target) {
  const std::string kFunction = R"(
    // This function observes the DOM for an attached form-control element `id`.
    //
    // On the first `change` event of such an element, it stores that element's
    // value in an array `window[observedValueSlots]`.
    //
    // Returns the index of that value.
    function listenForValueChange(id, unblockVariable) {
      console.log(`listenForValueChange('${id}')`);

      if (window.observedValueSlots === undefined)
        window.observedValueSlots = [];

      const waiterId = window.observedValueSlots.length;
      window.observedValueSlots.push(undefined);

      let observer = undefined;

      function changeHandler() {
        console.log(`listenForValueChange('${id}'): changeHandler()`);
        // Since other handlers may manipulate the fields value or remove it
        // from the DOM or replace it, we delay its execution.
        setTimeout(function() {
          console.log(`listenForValueChange('${id}'): changeHandler() timer`);
          if (unblockVariable && window[unblockVariable] !== true) {
            console.log(`listenForValueChange('${id}'): `+
                        `observed change, blocked by '${unblockVariable}'`);
            return;
          }
          const e = document.getElementById(id);
          if (e === null) {
            console.log(`listenForValueChange('${id}'): element not found`);
            return;
          }
          if (e.value === '') {
            console.log(`listenForValueChange('${id}'): empty value`);
            return;
          }
          console.log(`listenForValueChange('${id}'): storing in slot`);
          window.observedValueSlots[waiterId] = e.value;
          e.removeEventListener('change', changeHandler);
          observer.disconnect();
        }, 0);
      }

      // Observes the DOM to see if a new element `id` is added or some element
      // changes its ID to `id`.
      observer = new MutationObserver(function(mutations) {
        const e = document.getElementById(id);
        if (e !== null) {
          console.log(`listenForValueChange('${id}'): some element has been `+
                      `attached or change`);
          e.addEventListener('change', changeHandler);
        }
      });
      observer.observe(document, {
        attributes: true,
        childList: true,
        characterData: false,
        subtree: true
      });

      const e = document.getElementById(id);
      if (e !== null) {
        console.log(`listenForValueChange('${id}'): element exists already`);
        e.addEventListener('change', changeHandler);
      }

      return waiterId;
    }
  )";
  std::string call =
      base::StringPrintf("listenForValueChange(`%s`, `%s`)", id.c_str(),
                         unblock_variable.value_or("").c_str());
  content::EvalJsResult r = content::EvalJs(execution_target, kFunction + call);
  int waiterId = r.ExtractInt();
  return ValueWaiter(waiterId, execution_target);
}

}  // namespace

// Test fixtures derive from this class. This class hierarchy allows test
// fixtures to have distinct list of test parameters.
//
// TODO(crbug.com/832707): Parametrize this class to ensure that all tests in
//                         this run with all possible valid combinations of
//                         features and field trials.
class AutofillInteractiveTestBase : public AutofillUiTest {
 public:
  AutofillInteractiveTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Disable AutofillPageLanguageDetection because due to the little text in
    // the HTML files, the detected language is flaky (e.g., it often detects
    // "fr" instead of "en").
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillTextAreaChangeEvents},
        /*disabled_features=*/{features::kAutofillPageLanguageDetection});
  }
  ~AutofillInteractiveTestBase() override = default;

  AutofillInteractiveTestBase(const AutofillInteractiveTestBase&) = delete;
  AutofillInteractiveTestBase& operator=(const AutofillInteractiveTestBase&) =
      delete;

  bool IsPopupShown() {
    return !!ChromeAutofillClient::FromWebContentsForTesting(GetWebContents())
                 ->popup_controller_for_testing();
  }

  std::vector<FieldValue> GetFormValues(
      const ElementExpr& form = GetElementById("shipping")) {
    return GetFieldValues(ElementExpr(*form + ".elements"), GetWebContents());
  }

  base::RepeatingClosure ExpectValues(
      const std::vector<FieldValue>& expected_values,
      const ElementExpr& form = GetElementById("shipping")) {
    return base::BindRepeating(
        [](AutofillInteractiveTestBase* self,
           const std::vector<FieldValue>& expected_values,
           const ElementExpr& form) {
          EXPECT_THAT(self->GetFormValues(form), ValuesAre(expected_values));
        },
        this, expected_values, form);
  }

  content::EvalJsResult GetFieldValueById(const std::string& field_id) {
    return GetFieldValue(GetElementById(field_id));
  }

  content::EvalJsResult GetFieldCheckedById(const std::string& field_id) {
    return GetFieldChecked(GetElementById(field_id), GetWebContents());
  }

  content::EvalJsResult GetFieldValue(ElementExpr e) {
    return GetFieldValue(e, GetWebContents());
  }

  content::EvalJsResult GetFieldValue(
      const ElementExpr& e,
      content::ToRenderFrameHost execution_target) {
    std::string script = base::StringPrintf("%s.value", e->c_str());
    return content::EvalJs(execution_target, script);
  }

  content::EvalJsResult GetFieldChecked(
      const ElementExpr& e,
      content::ToRenderFrameHost execution_target) {
    std::string script = base::StringPrintf("%s.checked", e->c_str());
    return content::EvalJs(execution_target, script);
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    AutofillUiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    AutofillUiTest::SetUpOnMainThread();

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &AutofillInteractiveTestBase::HandleTestURL, base::Unretained(this)));
    ASSERT_TRUE(https_server_.InitializeAndListen());
    https_server_.StartAcceptingConnections();

    controllable_http_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/mock_translate_script.js",
            true /*relative_url_is_prefix*/);

    // Ensure that |embedded_test_server()| serves both domains used below.
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AutofillInteractiveTestBase::HandleTestURL, base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();

    // By default, all SSL cert checks are valid. Can be overriden in tests if
    // needed.
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillUiTest::SetUpCommandLine(command_line);
    cert_verifier_.SetUpCommandLine(command_line);
    // Needed to allow input before commit on various builders.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
    // TODO(crbug.com/1258185): Migrate to a better mechanism for testing around
    // language detection.
    command_line->AppendSwitch(switches::kOverrideLanguageDetection);
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
    if (!base::Contains(path_keyed_response_bodies_, request.relative_url)) {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html;charset=utf-8");
    response->set_content(path_keyed_response_bodies_[request.relative_url]);
    return std::move(response);
  }

  translate::LanguageState& GetLanguageState() {
    ChromeTranslateClient* client = ChromeTranslateClient::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    return *client->GetTranslateManager()->GetLanguageState();
  }

  // This is largely a copy of CheckForTranslateUI() from Translate's
  // translate_language_browsertest.cc.
  void NavigateToContentAndWaitForLanguageDetection(const char* content) {
    ASSERT_TRUE(browser());
    auto waiter = CreateTranslateWaiter(
        browser()->tab_strip_model()->GetActiveWebContents(),
        translate::TranslateWaiter::WaitEvent::kLanguageDetermined);

    SetTestUrlResponse(content);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));
    waiter->Wait();

    // Language detection sometimes fires early with an "und" (= undetermined)
    // detected code.
    size_t wait_counter = 0;
    constexpr size_t kMaxWaits = 2;
    while (GetLanguageState().source_language() == "und" ||
           GetLanguageState().source_language().empty()) {
      ++wait_counter;
      ASSERT_LE(wait_counter, kMaxWaits)
          << "Translate reported no/undetermined language " << wait_counter
          << " times";
      CreateTranslateWaiter(
          browser()->tab_strip_model()->GetActiveWebContents(),
          translate::TranslateWaiter::WaitEvent::kLanguageDetermined)
          ->Wait();
    }

    const TranslateBubbleModel* model =
        translate::test_utils::GetCurrentModel(browser());
    ASSERT_NE(nullptr, model);
  }

  // This is largely a copy of Translate() from Translate's
  // translate_language_browsertest.cc.
  void Translate(const bool first_translate) {
    auto waiter = CreateTranslateWaiter(
        browser()->tab_strip_model()->GetActiveWebContents(),
        translate::TranslateWaiter::WaitEvent::kPageTranslated);

    EXPECT_EQ(
        TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
        translate::test_utils::GetCurrentModel(browser())->GetViewState());

    translate::test_utils::PressTranslate(browser());
    if (first_translate)
      SimulateURLFetch();

    waiter->Wait();
    EXPECT_EQ(
        TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
        translate::test_utils::GetCurrentModel(browser())->GetViewState());
  }

  void CreateTestProfile() {
    AutofillProfile profile;
    test::SetProfileInfo(
        &profile, kDefaultAddressValues.first_name,
        kDefaultAddressValues.middle_name, kDefaultAddressValues.last_name,
        kDefaultAddressValues.email, kDefaultAddressValues.company,
        kDefaultAddressValues.address1, kDefaultAddressValues.address2,
        kDefaultAddressValues.city, kDefaultAddressValues.state,
        kDefaultAddressValues.zip, kDefaultAddressValues.country,
        kDefaultAddressValues.phone);
    profile.set_use_count(9999999);  // We want this to be the first profile.
    AddTestProfile(browser()->profile(), profile);
  }

  void CreateSecondTestProfile() {
    AutofillProfile profile;
    test::SetProfileInfo(&profile, "Alice", "M.", "Wonderland",
                         "alice@wonderland.com", "Magic", "333 Cat Queen St.",
                         "Rooftop", "Liliput", "CA", "10003", "US",
                         "15166900292");
    AddTestProfile(browser()->profile(), profile);
  }

  void CreateTestCreditCart() {
    CreditCard card;
    test::SetCreditCardInfo(&card, "Milton Waddams", "4111111111111111", "09",
                            "2999", "");
    AddTestCreditCard(browser()->profile(), card);
  }

  void SimulateURLFetch() {
    std::string script = R"(
        var google = {};
        google.translate = (function() {
          return {
            TranslateService: function() {
              return {
                isAvailable : function() {
                  return true;
                },
                restore : function() {
                  return;
                },
                getDetectedLanguage : function() {
                  return "ja";
                },
                translatePage : function(sourceLang, targetLang,
                                         onTranslateProgress) {
                  document.getElementsByTagName("body")[0].innerHTML = `)" +
                         std::string(kTestShippingFormString) + R"(`;
                  onTranslateProgress(100, true, false);
                }
              };
            }
          };
        })();
        cr.googleTranslate.onTranslateElementLoad(); )";

    controllable_http_response_->WaitForRequest();
    controllable_http_response_->Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/javascript\r\n"
        "\r\n");
    controllable_http_response_->Send(script);
    controllable_http_response_->Done();
  }

  // Make a pointless round trip to the renderer, giving the popup a chance to
  // show if it's going to. If it does show, an assert in
  // BrowserAutofillManagerTestDelegateImpl will trigger.
  void MakeSurePopupDoesntAppear() {
    EXPECT_EQ(42, content::EvalJs(GetWebContents(), "42"));
  }

  void FillElementWithValue(const std::string& element_id,
                            const std::string& value) {
    // Sends "|element_id|:|value|" to |msg_queue| if the |element_id|'s
    // value has changed to |value|.
    std::string script = base::StringPrintf(
        R"( (function() {
              const element_id = '%s';
              const value = '%s';
              const field = document.getElementById(element_id);
              const listener = function() {
                if (field.value === value) {
                  field.removeEventListener('input', listener);
                  domAutomationController.send(element_id +':'+ field.value);
                }
              };
              field.addEventListener('input', listener, false);
              return 'done';
            })(); )",
        element_id.c_str(), value.c_str());
    ASSERT_TRUE(content::ExecJs(GetWebContents(), script));

    content::DOMMessageQueue msg_queue(GetWebContents());
    for (char16_t character : value) {
      ui::DomKey dom_key = ui::DomKey::FromCharacter(character);
      const ui::PrintableCodeEntry* code_entry = base::ranges::find_if(
          ui::kPrintableCodeMap,
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
    std::string reply;
    ASSERT_TRUE(msg_queue.WaitForMessage(&reply));
    ASSERT_EQ("\"" + element_id + ":" + value + "\"", reply);
  }

  void DeleteElementValue(const ElementExpr& field) {
    std::string script = base::StringPrintf("%s.value = '';", field->c_str());
    ASSERT_TRUE(content::ExecJs(GetWebContents(), script));
    ASSERT_EQ("", GetFieldValue(field));
  }

  void ExecuteScript(const std::string& script) {
    ASSERT_TRUE(content::ExecJs(GetWebContents(), script));
  }

  GURL GetTestUrl() const { return https_server_.GetURL(kTestUrlPath); }

  void SetTestUrlResponse(std::string content) {
    SetResponseForUrlPath(kTestUrlPath, std::move(content));
  }

  void SetResponseForUrlPath(std::string path, std::string content) {
    path_keyed_response_bodies_[std::move(path)] = std::move(content);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  static const char kTestUrlPath[];

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  net::EmbeddedTestServer https_server_;

  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  content::ContentMockCertVerifier cert_verifier_;

  // KeyPressEventCallback that serves as a sink to ensure that every key press
  // event the tests create and have the WebContents forward is handled by some
  // key press event callback. It is necessary to have this sink because if no
  // key press event callback handles the event (at least on Mac), a DCHECK
  // ends up going off that the |event| doesn't have an |os_event| associated
  // with it.
  content::RenderWidgetHost::KeyPressEventCallback key_press_event_sink_;

  std::unique_ptr<net::test_server::ControllableHttpResponse>
      controllable_http_response_;

  // A map of relative paths to content that shall be served with an HTTP_OK
  // response. If the map contains no entry, the request falls through to the
  // serving from disk.
  std::map<std::string, std::string> path_keyed_response_bodies_;

  base::test::ScopedFeatureList feature_list_;

  base::HistogramTester histogram_tester_;
};

const char AutofillInteractiveTestBase::kTestUrlPath[] =
    "/internal/test_url_path";

class AutofillInteractiveTest : public AutofillInteractiveTestBase {
 protected:
  AutofillInteractiveTest()
      : feature_list_(features::kAutofillEnableSelectList) {}
  ~AutofillInteractiveTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillInteractiveTestBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        translate::switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
    command_line->AppendSwitchASCII("enable-blink-features",
                                    "HTMLSelectListElement");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AutofillInteractiveTestWithHistogramTester
    : public AutofillInteractiveTest {
 public:
  AutofillInteractiveTestWithHistogramTester() {
    feature_list_.InitWithFeatureState(
        features::test::kAutofillServerCommunication, true);
  }

  void SetUp() override {
    url_loader_interceptor_ = std::make_unique<URLLoaderInterceptor>(
        base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
          // Only allow requests to be loaded that are necessary for the test.
          // This allows a histogram to test properties of some specific
          // requests.
          std::vector<std::string> allowlist = {
              "/internal/test_url_path", "https://clients1.google.com/tbproxy",
              "https://content-autofill.googleapis.com/"};
          // Intercept if not allow-listed.
          return base::ranges::all_of(allowlist, [&params](const auto& s) {
            return params->url_request.url.spec().find(s) == std::string::npos;
          });
        }));
    AutofillInteractiveTest::SetUp();
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    AutofillInteractiveTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillInteractiveTest::SetUpCommandLine(command_line);
    // Prevents proxy.pac requests.
    command_line->AppendSwitch(switches::kNoProxyServer);
  }

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  base::test::ScopedFeatureList feature_list_;
};

// Test the basic form-fill flow.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, BasicFormFill) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));
}

// Test that hidden selects get filled. Hidden selects are often used by widgets
// which look like <select>s but are actually constructed out of divs.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, FillHiddenSelect) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/form_hidden_select.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  // Make sure the form was filled correctly.
  EXPECT_EQ(kDefaultAddressValues.first_name, GetFieldValueById("firstname"));
  EXPECT_EQ(kDefaultAddressValues.address1, GetFieldValueById("address1"));
  EXPECT_EQ(kDefaultAddressValues.city, GetFieldValueById("city"));
  EXPECT_EQ(kDefaultAddressValues.state_short, GetFieldValueById("state"));
}

// AutofillInteractiveTest subclass which disables autofilling <selectlist>.
class AutofillInteractiveDisableAutofillSelectListTest
    : public AutofillInteractiveTest {
 protected:
  AutofillInteractiveDisableAutofillSelectListTest() {
    feature_list_.InitAndDisableFeature(features::kAutofillEnableSelectList);
  }
  ~AutofillInteractiveDisableAutofillSelectListTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the <selectlist> is not filled if the <selectlist> autofilling
// feature is disabled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveDisableAutofillSelectListTest,
                       DisableSelectListAutofilling) {
  const char kFormWithSelectListString[] = R"(
    <!-- Disable extra network request for /favicon.ico -->
    <link rel="icon" href="data:,">
    <form action="https://www.example.com/" method="POST" id="shipping">
      <label for="firstname">First name:</label>
      <input type="text" id="firstname" autocomplete="given-name"><br>
      <label for="state">State:</label>
      <selectlist id="state" autocomplete="address-level1">
        <option value="" selected="yes">--</option>
        <option value="CA">California</option>
        <option value="TX">Texas</option>
      </selectlist>
    </form>
    )";

  CreateTestProfile();
  SetTestUrlResponse(kFormWithSelectListString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  EXPECT_THAT(GetFormValues(),
              ValuesAre({{"firstname", kDefaultAddressValues.first_name},
                         {"state", ""}}));
}

class AutofillInteractiveTest_UndoAutofill : public AutofillInteractiveTest {
  base::test::ScopedFeatureList scoped_feature_list_{features::kAutofillUndo};
};

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest_UndoAutofill,
                       BasicUndoAutofill) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.target_index = 1}));

  std::vector<FieldValue> expected_values = kEmptyAddress;
  expected_values[0].value = "M";
  EXPECT_THAT(GetFormValues(), ValuesAre(expected_values));
}

class AutofillInteractiveTest_ClearForm : public AutofillInteractiveTest {
 public:
  AutofillInteractiveTest_ClearForm() {
    scoped_feature_list_.InitAndDisableFeature(features::kAutofillUndo);
  }
  ~AutofillInteractiveTest_ClearForm() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest_ClearForm, BasicClear) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.target_index = 1}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kEmptyAddress));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest_ClearForm, ClearTwoSection) {
  static const char kTestBillingFormString[] =
      R"( An example of a billing address form.
          <form action="https://www.example.com/" method="POST" id="billing">
          <label for="firstname_billing">First name:</label>
           <input type="text" id="firstname_billing"><br>
          <label for="lastname_billing">Last name:</label>
           <input type="text" id="lastname_billing"><br>
          <label for="address1_billing">Address line 1:</label>
           <input type="text" id="address1_billing"><br>
          <label for="address2_billing">Address line 2:</label>
           <input type="text" id="address2_billing"><br>
          <label for="city_billing">City:</label>
           <input type="text" id="city_billing"><br>
          <label for="state_billing">State:</label>
           <select id="state_billing">
           <option value="" selected="yes">--</option>
           <option value="CA">California</option>
           <option value="TX">Texas</option>
           </select><br>
          <label for="zip_billing">ZIP code:</label>
           <input type="text" id="zip_billing"><br>
          <label for="country_billing">Country:</label>
           <select id="country_billing">
           <option value="" selected="yes">--</option>
           <option value="CA">Canada</option>
           <option value="US">United States</option>
           </select><br>
          <label for="phone_billing">Phone number:</label>
           <input type="text" id="phone_billing"><br>
          </form> )";
  CreateTestProfile();
  SetTestUrlResponse(
      base::StrCat({kTestShippingFormString, kTestBillingFormString}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Fill shipping form.
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(GetElementById("shipping")),
              ValuesAre(kDefaultAddress));
  EXPECT_THAT(GetFormValues(GetElementById("billing")),
              ValuesAre(kEmptyAddress));

  // Fill billing form.
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_billing"), this));
  EXPECT_THAT(GetFormValues(GetElementById("billing")),
              ValuesAre(kDefaultAddress));
  EXPECT_THAT(GetFormValues(GetElementById("shipping")),
              ValuesAre(kDefaultAddress));

  // Clear billing form.
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_billing"), this,
                           {.target_index = 1}));
  EXPECT_THAT(GetFormValues(GetElementById("shipping")),
              ValuesAre(kDefaultAddress));
  EXPECT_THAT(GetFormValues(GetElementById("billing")),
              ValuesAre(kEmptyAddress));
}

// TODO(crbug.com/1468282) Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ModifyTextFieldAndFill DISABLED_ModifyTextFieldAndFill
#else
#define MAYBE_ModifyTextFieldAndFill ModifyTextFieldAndFill
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       MAYBE_ModifyTextFieldAndFill) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Modify a field.
  ASSERT_TRUE(FocusField(GetElementById("city"), GetWebContents()));
  FillElementWithValue("city", "Montreal");
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ModifyTextNotifiesObserver) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  autofill::MockAutofillManagerObserver observer;
  BrowserAutofillManager* autofill_manager = GetBrowserAutofillManager();
  autofill_manager->AddObserver(&observer);

  // OnAfterTextFieldDidChange will eventually be called with the final text
  // "Montreal".
  EventWaiter<bool> waiter({true});
  EXPECT_CALL(observer, OnAfterTextFieldDidChange(_, _, _, _))
      .WillRepeatedly([&](AutofillManager&, FormGlobalId, FieldGlobalId,
                          std::u16string text_value) {
        if (text_value == u"Montreal") {
          waiter.OnEvent(true);
        }
      });

  ASSERT_TRUE(FocusField(GetElementById("city"), GetWebContents()));
  FillElementWithValue("city", "Montreal");

  ASSERT_TRUE(waiter.Wait());
  autofill_manager->RemoveObserver(&observer);
}

// Same as ModifyTextNotifiesObserver, but for textarea rather than input
// elements.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       ModifyTextAreaNotifiesObserver) {
  constexpr char kForm[] = R"(
  <html>
  <head>
    <!-- Disable extra network request for /favicon.ico -->
    <link rel="icon" href="data:,">
  </head>
  <body>
    <form action="https://www.example.com/" method="POST" id="shipping">
    <label for="address1">Address line 1:</label>
     <textarea id="address1"></textarea>
    </form>
    )";

  CreateTestProfile();
  SetTestUrlResponse(kForm);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  autofill::MockAutofillManagerObserver observer;
  BrowserAutofillManager* autofill_manager = GetBrowserAutofillManager();
  autofill_manager->AddObserver(&observer);

  EventWaiter<bool> waiter({true});
  EXPECT_CALL(observer, OnAfterTextFieldDidChange(_, _, _, _))
      .WillRepeatedly([&](AutofillManager&, FormGlobalId, FieldGlobalId,
                          std::u16string text_value) {
        if (text_value == u"My Address") {
          waiter.OnEvent(true);
        }
      });

  ASSERT_TRUE(FocusField(GetElementById("address1"), GetWebContents()));
  FillElementWithValue("address1", "My Address");

  ASSERT_TRUE(waiter.Wait());
  autofill_manager->RemoveObserver(&observer);
}

void DoModifySelectFieldAndFill(AutofillInteractiveTest* test,
                                bool should_test_selectlist) {
  test->CreateTestProfile();
  test->SetTestUrlResponse(should_test_selectlist
                               ? GenerateTestShippingFormWithSelectList()
                               : kTestShippingFormString);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(test->browser(), test->GetTestUrl()));

  // Modify a field.
  ASSERT_TRUE(FocusField(GetElementById("state"), test->GetWebContents()));
  ASSERT_NE(kDefaultAddressValues.state_short, base::StringPiece("CA"));
  test->FillElementWithValue("state", "CA");

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), test));
  EXPECT_THAT(test->GetFormValues(),
              ValuesAre(MergeValue(kDefaultAddress, {"state", "CA"})));
}

// Test that autofill doesn't refill a <select> field initially modified by the
// user.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ModifySelectFieldAndFill) {
  DoModifySelectFieldAndFill(this, /*should_test_selectlist=*/false);
}

// Test that autofill doesn't refill a <selectlist> field initially modified by
// the user.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ModifySelectListFieldAndFill) {
  DoModifySelectFieldAndFill(this, /*should_test_selectlist=*/true);
}

// Test that autofill works when the website prefills the form when
// |kAutofillPreventOverridingPrefilledValues| is not enabled, otherwise, the
// prefilled field values are not overridden.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, PrefillFormAndFill) {
  const char kPrefillScript[] =
      R"( <script>
            document.getElementById('firstname').value = 'Seb';
            document.getElementById('lastname').value = 'Bell';
            document.getElementById('address1').value = '3243 Notre-Dame Ouest';
            document.getElementById('address2').value = 'apt 843';
            document.getElementById('city').value = 'Montreal';
            document.getElementById('zip').value = 'H5D 4D3';
            document.getElementById('phone').value = '15142223344';
          </script>)";
  CreateTestProfile();
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kPrefillScript}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // We need to delete the prefilled value and then trigger the autofill.
  auto Delete = [this] { DeleteElementValue(GetElementById("firstname")); };
  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this,
                   {.after_focus = base::BindLambdaForTesting(Delete)}));
  if (base::FeatureList::IsEnabled(
          features::kAutofillPreventOverridingPrefilledValues)) {
    EXPECT_EQ("Milton", GetFieldValueById("firstname"));
    EXPECT_EQ("Bell", GetFieldValueById("lastname"));
    EXPECT_EQ("3243 Notre-Dame Ouest", GetFieldValueById("address1"));
    EXPECT_EQ("apt 843", GetFieldValueById("address2"));
    EXPECT_EQ("Montreal", GetFieldValueById("city"));
    EXPECT_EQ("H5D 4D3", GetFieldValueById("zip"));
    EXPECT_EQ("15142223344", GetFieldValueById("phone"));
  } else {
    EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));
  }
}

// Test that autofill doesn't refill a field modified by the user.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest_ClearForm,
                       FillChangeSecondFieldRefillAndClearFirstFill) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  // Change the last name.
  ASSERT_TRUE(FocusField(GetElementById("lastname"), GetWebContents()));
  ASSERT_TRUE(SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                                   {ObservedUiEvents::kSuggestionsShown}));
  ASSERT_TRUE(SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                                   {ObservedUiEvents::kSuggestionsShown}));
  EXPECT_THAT(GetFormValues(),
              ValuesAre(MergeValue(kDefaultAddress, {"lastname", "Wadda"})));

  // Fill again by focusing on the first field.
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  EXPECT_THAT(GetFormValues(),
              ValuesAre(MergeValue(kDefaultAddress, {"lastname", "Wadda"})));

  // Clear everything except last name by selecting 'clear' on the first field.
  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.target_index = 1}));
  EXPECT_THAT(GetFormValues(),
              ValuesAre(MergeValue(kEmptyAddress, {"lastname", "Wadda"})));
}

// Test that multiple autofill operations work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest_ClearForm,
                       FillChangeSecondFieldRefillAndClearSecondField) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  // Change the last name.
  ASSERT_TRUE(FocusField(GetElementById("lastname"), GetWebContents()));
  ASSERT_TRUE(SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                                   {ObservedUiEvents::kSuggestionsShown}));
  ASSERT_TRUE(SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                                   {ObservedUiEvents::kSuggestionsShown}));
  EXPECT_THAT(GetFormValues(),
              ValuesAre(MergeValue(kDefaultAddress, {"lastname", "Wadda"})));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.do_focus = false,
                            .do_show = false,
                            .show_method = ShowMethod::ByChar('M')}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.target_index = 1}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kEmptyAddress));
}

// Test that multiple autofill operations work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest_ClearForm,
                       FillChangeSecondFieldRefillSecondFieldClearFirst) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  // Change the last name.
  ASSERT_TRUE(FocusField(GetElementById("lastname"), GetWebContents()));
  ASSERT_TRUE(SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                                   {ObservedUiEvents::kSuggestionsShown}));
  ASSERT_TRUE(SendKeyToPageAndWait(ui::DomKey::BACKSPACE,
                                   {ObservedUiEvents::kSuggestionsShown}));
  EXPECT_THAT(GetFormValues(),
              ValuesAre(MergeValue(kDefaultAddress, {"lastname", "Wadda"})));

  ASSERT_TRUE(AutofillFlow(GetElementById("lastname"), this));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.target_index = 1}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kEmptyAddress));
}

// Test that multiple autofill operations work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest_ClearForm,
                       FillThenFillSomeWithAnotherProfileThenClear) {
  CreateTestProfile();
  CreateSecondTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .num_profile_suggestions = 2,
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  // Delete some fields.
  ASSERT_TRUE(FocusField(GetElementById("city"), GetWebContents()));
  DeleteElementValue(GetElementById("city"));
  ASSERT_TRUE(AutofillFlow(GetElementById("address1"), this,
                           {.num_profile_suggestions = 2,
                            .target_index = 1,
                            .after_focus = base::BindLambdaForTesting([&]() {
                              DeleteElementValue(GetElementById("address1"));
                            })}));
  // Address line 1 and city from the second profile.
  EXPECT_THAT(
      GetFormValues(),
      ValuesAre(MergeValues(kDefaultAddress, {{"address1", "333 Cat Queen St."},
                                              {"city", "Liliput"}})));

  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.target_index = 1}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kEmptyAddress));
}

// Test that form filling can be initiated by pressing the down arrow.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillViaDownArrow) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Focus a fillable field.
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  // The form should be filled.
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillSelectViaTab) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Focus a fillable field.
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  // The form should be filled.
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillViaClick) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByClick(),
                            .execution_target = GetWebContents()}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));
}

// Makes sure that the first click does or does not activate the autofill popup
// on the initial click within a fillable field.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, Click) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.do_focus = false,
                            .show_method = ShowMethod::ByClick(),
                            .execution_target = GetWebContents()}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));
}

// Makes sure that clicking outside the focused field doesn't activate
// the popup.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DontAutofillForOutsideClick) {
  static const char kDisabledButton[] =
      R"(<button disabled id='disabled-button'>Cant click this</button>)";
  CreateTestProfile();
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kDisabledButton}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Clicking a disabled button will generate a mouse event but focus doesn't
  // change. This tests that autofill can handle a mouse event outside a focused
  // input *without* showing the popup.
  ASSERT_FALSE(AutofillFlow(GetElementById("disabled-button"), this,
                            {.do_focus = false,
                             .do_select = false,
                             .do_accept = false,
                             .show_method = ShowMethod::ByClick(),
                             .execution_target = GetWebContents()}));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.do_focus = false,
                            .show_method = ShowMethod::ByClick(),
                            .execution_target = GetWebContents()}));
}

// Makes sure that clicking a field while there is no enough height in the
// content area for at least one suggestion, won't show the autofill popup. This
// is a regression test for crbug.com/1108181
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DontAutofillShowPopupWhenNoEnoughHeightInContentArea) {
  // This firstname field starts at y=-100px and has a height of 5120px. There
  // is no enough space to show at least one row of the autofill popup and hence
  // the autofill shouldn't be shown.
  static const char kTestFormWithLargeInputField[] =
      R"(<form action="https://www.example.com/" method="POST">
         <label for="firstname">First name:</label>
         <input type="text" id="firstname" style="position:fixed;
           top:-100px;height:5120px"><br>
         <label for="lastname">Last name:</label>
         <input type="text" id="lastname"><br>
         <label for="city">City:</label>
         <input type="text" id="city"><br>
         </form>)";
  CreateTestProfile();
  SetTestUrlResponse(kTestFormWithLargeInputField);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_FALSE(AutofillFlow(GetElementById("firstname"), this,
                            {.do_select = false,
                             .do_accept = false,
                             .show_method = ShowMethod::ByClick(),
                             // Since failure is expected, no need to retry
                             // showing the Autofill popup too often.
                             .max_show_tries = 2,
                             .execution_target = GetWebContents()}));
}

// Test that a field is still autofillable after the previously autofilled
// value is deleted.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnDeleteValueAfterAutofill) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M')}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  // Delete the value of a filled field.
  DeleteElementValue(GetElementById("firstname"));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M')}));
  EXPECT_EQ("Milton", GetFieldValue(GetElementById("firstname")));
}

// Test that an input field is not rendered with the blue autofilled
// background color when choosing an option from the datalist suggestion list.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnSelectOptionFromDatalist) {
  static const char kTestForm[] =
      R"( <p>The text is some page content to paint</p>
          <form action="https://www.example.com/" method="POST">
            <input list="dl" type="search" id="firstname"><br>
            <datalist id="dl">
            <option value="Adam"></option>
            <option value="Bob"></option>
            <option value="Carl"></option>
            </datalist>
          </form> )";
  SetTestUrlResponse(kTestForm);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  auto GetBackgroundColor = [this](const ElementExpr& id) {
    std::string script = base::StringPrintf(
        "document.defaultView.getComputedStyle(%s).backgroundColor",
        id->c_str());
    return content::EvalJs(GetWebContents(), script).ExtractString();
  };
  std::string original_color = GetBackgroundColor(GetElementById("firstname"));
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.num_profile_suggestions = 0, .target_index = 1}));
  EXPECT_EQ("Bob", GetFieldValueById("firstname"));
  EXPECT_EQ(GetBackgroundColor(GetElementById("firstname")), original_color);
}

// Test that an <input> field with a <datalist> has a working drop down even if
// it was dynamically changed to <input type="password"> temporarily. This is a
// regression test for crbug.com/918351.
IN_PROC_BROWSER_TEST_F(
    AutofillInteractiveTest,
    OnSelectOptionFromDatalistTurningToPasswordFieldAndBack) {
  static const char kTestForm[] =
      R"( <p>The text is some page content to paint</p>
          <form action="https://www.example.com/" method="POST">
            <input list="dl" type="search" id="firstname"><br>
            <datalist id="dl">
            <option value="Adam"></option>
            <option value="Bob"></option>
            <option value="Carl"></option>
            </datalist>
          </form> )";
  SetTestUrlResponse(kTestForm);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "document.getElementById('firstname').type = 'password';"));
  // At this point, the IsPasswordFieldForAutofill() function returns true and
  // will continue to return true for the field, even when the type is changed
  // back to 'search'.
  ASSERT_TRUE(
      content::ExecJs(GetWebContents(),
                      "document.getElementById('firstname').type = 'search';"));

  // Regression test for crbug.com/918351 whether the datalist becomes available
  // again.
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.num_profile_suggestions = 0, .target_index = 1}));
  // Pressing the down arrow preselects the first item. Pressing it again
  // selects the second item.
  EXPECT_EQ("Bob", GetFieldValueById("firstname"));
}

// Test that a JavaScript oninput event is fired after auto-filling a form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnInputAfterAutofill) {
  static const char kOnInputScript[] =
      R"( <script>
          focused_fired = false;
          unfocused_fired = false;
          changed_select_fired = false;
          unchanged_select_fired = false;
          document.getElementById('firstname').oninput = function() {
            focused_fired = true;
          };
          document.getElementById('lastname').oninput = function() {
            unfocused_fired = true;
          };
          document.getElementById('state').oninput = function() {
            changed_select_fired = true;
          };
          document.getElementById('country').oninput = function() {
            unchanged_select_fired = true;
          };
          document.getElementById('country').value = 'US';
          </script> )";
  CreateTestProfile();
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kOnInputScript}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M')}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "focused_fired;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "unfocused_fired;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "changed_select_fired;"));
  EXPECT_EQ(false,
            content::EvalJs(GetWebContents(), "unchanged_select_fired;"));
}

// Test that a JavaScript onchange event is fired after auto-filling a form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, OnChangeAfterAutofill) {
  static const char kOnChangeScript[] =
      R"( <script>
          focused_fired = false;
          unfocused_fired = false;
          changed_select_fired = false;
          unchanged_select_fired = false;
          document.getElementById('firstname').onchange = function() {
            focused_fired = true;
          };
          document.getElementById('lastname').onchange = function() {
            unfocused_fired = true;
          };
          document.getElementById('state').onchange = function() {
            changed_select_fired = true;
          };
          document.getElementById('country').onchange = function() {
            unchanged_select_fired = true;
          };
          document.getElementById('country').value = 'US';
          </script> )";
  CreateTestProfile();
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kOnChangeScript}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M')}));

  // The form should be filled.
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "focused_fired;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "unfocused_fired;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "changed_select_fired;"));
  EXPECT_EQ(false,
            content::EvalJs(GetWebContents(), "unchanged_select_fired;"));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, InputFiresBeforeChange) {
  static const char kInputFiresBeforeChangeScript[] =
      R"(<script>
         inputElementEvents = [];
         function recordInputElementEvent(e) {
           if (e.target.tagName != 'INPUT') throw 'only <input> tags allowed';
           inputElementEvents.push(e.type);
         }
         selectElementEvents = [];
         function recordSelectElementEvent(e) {
           if (e.target.tagName != 'SELECT') throw 'only <select> tags allowed';
           selectElementEvents.push(e.type);
         }
         document.getElementById('lastname').oninput = recordInputElementEvent;
         document.getElementById('lastname').onchange = recordInputElementEvent;
         document.getElementById('country').oninput = recordSelectElementEvent;
         document.getElementById('country').onchange = recordSelectElementEvent;
         </script>)";
  CreateTestProfile();
  SetTestUrlResponse(
      base::StrCat({kTestShippingFormString, kInputFiresBeforeChangeScript}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  EXPECT_EQ(2, content::EvalJs(GetWebContents(), "inputElementEvents.length;"));

  std::vector<std::string> input_element_events = {
      content::EvalJs(GetWebContents(), "inputElementEvents[0];")
          .ExtractString(),
      content::EvalJs(GetWebContents(), "inputElementEvents[1];")
          .ExtractString(),
  };

  EXPECT_THAT(input_element_events, ElementsAre("input", "change"));

  EXPECT_EQ(2,
            content::EvalJs(GetWebContents(), "selectElementEvents.length;"));

  std::vector<std::string> select_element_events = {
      content::EvalJs(GetWebContents(), "selectElementEvents[0];")
          .ExtractString(),
      content::EvalJs(GetWebContents(), "selectElementEvents[1];")
          .ExtractString(),
  };

  EXPECT_THAT(select_element_events, ElementsAre("input", "change"));
}

// Test that we can autofill forms distinguished only by their |id| attribute.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       AutofillFormsDistinguishedById) {
  static const char kScript[] =
      R"( <script>
          var mainForm = document.forms[0];
          mainForm.id = 'mainForm';
          var newForm = document.createElement('form');
          newForm.action = mainForm.action;
          newForm.method = mainForm.method;
          newForm.id = 'newForm';
          mainForm.parentNode.insertBefore(newForm, mainForm);
          </script> )";
  CreateTestProfile();
  SetTestUrlResponse(base::StrCat({kTestShippingFormString, kScript}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(
                                MergeValue(kEmptyAddress, {"firstname", "M"}),
                                GetElementById("mainForm"))}));
  EXPECT_THAT(GetFormValues(GetElementById("mainForm")),
              ValuesAre(kDefaultAddress));
}

// Test that we properly autofill forms with repeated fields.
// In the wild, the repeated fields are typically either email fields
// (duplicated for "confirmation"); or variants that are hot-swapped via
// JavaScript, with only one actually visible at any given time.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillFormWithRepeatedField) {
  static const char kForm[] =
      R"( <form action="https://www.example.com/" method="POST">
          <label for="firstname">First name:</label>
           <input type="text" id="firstname"><br>
          <label for="lastname">Last name:</label>
           <input type="text" id="lastname"><br>
          <label for="address1">Address line 1:</label>
           <input type="text" id="address1"><br>
          <label for="address2">Address line 2:</label>
           <input type="text" id="address2"><br>
          <label for="city">City:</label>
           <input type="text" id="city"><br>
          <label for="state">State:</label>
           <select id="state">
           <option value="" selected="yes">--</option>
           <option value="CA">California</option>
           <option value="TX">Texas</option>
           </select><br>
          <label for="state_freeform" style="display:none">State:</label>
           <input type="text" id="state_freeform" style="display:none"><br>
          <label for="zip">ZIP code:</label>
           <input type="text" id="zip"><br>
          <label for="country">Country:</label>
           <select id="country">
           <option value="" selected="yes">--</option>
           <option value="CA">Canada</option>
           <option value="US">United States</option>
           </select><br>
          <label for="phone">Phone number:</label>
           <input type="text" id="phone"><br>
          </form> )";
  CreateTestProfile();
  SetTestUrlResponse(kForm);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  std::vector<FieldValue> empty = kEmptyAddress;
  empty.insert(empty.begin() + 6, {"state_freeform", ""});
  std::vector<FieldValue> filled = kDefaultAddress;
  filled.insert(filled.begin() + 6, {"state_freeform", ""});

  ASSERT_TRUE(AutofillFlow(
      GetElementById("firstname"), this,
      {.show_method = ShowMethod::ByChar('M'),
       .after_select = ExpectValues(MergeValue(empty, {"firstname", "M"}),
                                    ElementExpr("document.forms[0]"))}));
  EXPECT_THAT(GetFormValues(ElementExpr("document.forms[0]")),
              ValuesAre(filled));
}

// Test that we properly autofill forms with non-autofillable fields.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       AutofillFormWithAutocompleteOffField) {
  static const char kForm[] =
      R"( <form action="https://www.example.com/" method="POST">
          <label for="firstname">First name:</label>
           <input type="text" id="firstname"><br>
          <label for="middlename">Middle name:</label>
           <input type="text" id="middlename" autocomplete="off" /><br>
          <label for="lastname">Last name:</label>
           <input type="text" id="lastname"><br>
          <label for="address1">Address line 1:</label>
           <input type="text" id="address1"><br>
          <label for="address2">Address line 2:</label>
           <input type="text" id="address2"><br>
          <label for="city">City:</label>
           <input type="text" id="city"><br>
          <label for="state">State:</label>
           <select id="state">
           <option value="" selected="yes">--</option>
           <option value="CA">California</option>
           <option value="TX">Texas</option>
           </select><br>
          <label for="zip">ZIP code:</label>
           <input type="text" id="zip"><br>
          <label for="country">Country:</label>
           <select id="country">
           <option value="" selected="yes">--</option>
           <option value="CA">Canada</option>
           <option value="US">United States</option>
           </select><br>
          <label for="phone">Phone number:</label>
           <input type="text" id="phone"><br>
          </form> )";
  CreateTestProfile();
  SetTestUrlResponse(kForm);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  std::vector<FieldValue> empty = kEmptyAddress;
  empty.insert(empty.begin() + 1, {"middlename", ""});
  std::vector<FieldValue> filled = kDefaultAddress;
  filled.insert(filled.begin() + 1, {"middlename", "C."});

  ASSERT_TRUE(AutofillFlow(
      GetElementById("firstname"), this,
      {.show_method = ShowMethod::ByChar('M'),
       .after_select = ExpectValues(MergeValue(empty, {"firstname", "M"}),
                                    ElementExpr("document.forms[0]"))}));
  EXPECT_THAT(GetFormValues(ElementExpr("document.forms[0]")),
              ValuesAre(filled));
}

// Test that we can Autofill dynamically generated forms.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DynamicFormFill) {
  static const char kDynamicForm[] =
      R"( <p>Some text to paint</p>
          <form id="form" action="https://www.example.com/"
                method="POST"></form>
          <script>
          function AddElement(name, label) {
            var form = document.getElementById('form');

            var label_text = document.createTextNode(label);
            var label_element = document.createElement('label');
            label_element.setAttribute('for', name);
            label_element.appendChild(label_text);
            form.appendChild(label_element);

            if (name === 'state' || name === 'country') {
              var select_element = document.createElement('select');
              select_element.setAttribute('id', name);
              select_element.setAttribute('name', name);

              /* Add an empty selected option. */
              var default_option = new Option('--', '', true);
              select_element.appendChild(default_option);

              /* Add the other options. */
              if (name == 'state') {
                var option1 = new Option('California', 'CA');
                select_element.appendChild(option1);
                var option2 = new Option('Texas', 'TX');
                select_element.appendChild(option2);
              } else {
                var option1 = new Option('Canada', 'CA');
                select_element.appendChild(option1);
                var option2 = new Option('United States', 'US');
                select_element.appendChild(option2);
              }

              form.appendChild(select_element);
            } else {
              var input_element = document.createElement('input');
              input_element.setAttribute('id', name);
              input_element.setAttribute('name', name);

              form.appendChild(input_element);
            }

            form.appendChild(document.createElement('br'));
          };

          function BuildForm() {
            var elements = [
              ['firstname', 'First name:'],
              ['lastname', 'Last name:'],
              ['address1', 'Address line 1:'],
              ['address2', 'Address line 2:'],
              ['city', 'City:'],
              ['state', 'State:'],
              ['zip', 'ZIP code:'],
              ['country', 'Country:'],
              ['phone', 'Phone number:'],
            ];

            for (var i = 0; i < elements.length; i++) {
              var name = elements[i][0];
              var label = elements[i][1];
              AddElement(name, label);
            }
          };
          </script> )";
  CreateTestProfile();
  SetTestUrlResponse(kDynamicForm);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Dynamically construct the form.
  ASSERT_TRUE(content::ExecJs(GetWebContents(), "BuildForm();"));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(
                                MergeValue(kEmptyAddress, {"firstname", "M"}),
                                GetElementById("form"))}));
  EXPECT_THAT(GetFormValues(GetElementById("form")),
              ValuesAre(kDefaultAddress));
}

// Test that form filling works after reloading the current page.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillAfterReload) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // Reload the page.
  content::WebContents* web_contents = GetWebContents();
  web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));
}

// Test that filling a form sends all the expected events to the different
// fields being filled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillEvents) {
  // TODO(crbug.com/609861): Remove the autocomplete attribute from the textarea
  // field when the bug is fixed.
  static const char kTestEventFormString[] =
      R"( <script type="text/javascript">
          var inputfocus = false;
          var inputkeydown = false;
          var inputinput = false;
          var inputchange = false;
          var inputkeyup = false;
          var inputblur = false;
          var textfocus = false;
          var textkeydown = false;
          var textinput= false;
          var textchange = false;
          var textkeyup = false;
          var textblur = false;
          var selectfocus = false;
          var selectinput = false;
          var selectchange = false;
          var selectblur = false;
          var selectlistfocus = false;
          var selectlistinput = false;
          var selectlistchange = false;
          var selectlistblur = false;
          </script>
          A form for testing events.
          <form action="https://www.example.com/" method="POST" id="shipping">
          <label for="firstname">First name:</label>
           <input type="text" id="firstname"><br>
          <label for="lastname">Last name:</label>
           <input type="text" id="lastname"
           onfocus="inputfocus = true" onkeydown="inputkeydown = true"
           oninput="inputinput = true" onchange="inputchange = true"
           onkeyup="inputkeyup = true" onblur="inputblur = true" ><br>
          <label for="address1">Address line 1:</label>
           <input type="text" id="address1"><br>
          <label for="address2">Address line 2:</label>
           <input type="text" id="address2"><br>
          <label for="city">City:</label>
           <textarea rows="4" cols="50" id="city" name="city"
           autocomplete="address-level2" onfocus="textfocus = true"
           onkeydown="textkeydown = true" oninput="textinput = true"
           onchange="textchange = true" onkeyup="textkeyup = true"
           onblur="textblur = true"></textarea><br>
          <label for="state">State:</label>
           <select id="state"
           onfocus="selectfocus = true" oninput="selectinput = true"
           onchange="selectchange = true" onblur="selectblur = true" >
           <option value="" selected="yes">--</option>
           <option value="CA">California</option>
           <option value="NY">New York</option>
           <option value="TX">Texas</option>
           </select><br>
          <label for="zip">ZIP code:</label>
           <input type="text" id="zip"><br>
          <label for="country">Country:</label>
           <selectlist id="country"
           onfocus="selectlistfocus = true" oninput="selectlistinput = true"
           onchange="selectlistchange = true" onblur="selectlistblur = true" >
           <option value="" selected="yes">--</option>
           <option value="CA">Canada</option>
           <option value="US">United States</option>
           </selectlist><br>
          <label for="phone">Phone number:</label>
           <input type="text" id="phone"><br>
          </form> )";
  CreateTestProfile();
  SetTestUrlResponse(kTestEventFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  // Checks that all the events were fired for the input field.
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "inputfocus;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "inputkeydown;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "inputinput;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "inputchange;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "inputkeyup;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "inputblur;"));

  // Checks that all the events were fired for the textarea field.
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "textfocus;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "textkeydown;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "textinput;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "textchange;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "textkeyup;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "textblur;"));

  // Checks that all the events were fired for the select field.
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectfocus;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectinput;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectchange;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectblur;"));

  // Checks that all the events were fired for the selectlist field.
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectlistfocus;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectlistinput;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectlistchange;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectlistblur;"));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, AutofillAfterTranslate) {
  ASSERT_TRUE(TranslateService::IsTranslateBubbleEnabled());
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
  CreateTestProfile();

  static const char kForm[] =
      R"( <form action="https://www.example.com/" method="POST">
          <label for="fn">Nom</label>
           <input type="text" id="fn"><br>
          <label for="ln">Nom de famille</label>
           <input type="text" id="ln"><br>
          <label for="a1">Address line 1:</label>
           <input type="text" id="a1"><br>
          <label for="a2">Address line 2:</label>
           <input type="text" id="a2"><br>
          <label for="ci">City:</label>
           <input type="text" id="ci"><br>
          <label for="st">State:</label>
           <select id="st">
           <option value="" selected="yes">--</option>
           <option value="CA">California</option>
           <option value="TX">Texas</option>
           </select><br>
          <label for="z">ZIP code:</label>
           <input type="text" id="z"><br>
          <label for="co">Country:</label>
           <select id="co">
           <option value="" selected="yes">--</option>
           <option value="CA">Canada</option>
           <option value="US">United States</option>
           </select><br>
          <label for="ph">Phone number:</label>
           <input type="text" id="ph"><br>
          </form>
          Nous serons importants et intéressants, mais les épreuves et les
          peines peuvent lui en procurer de grandes en raison de situations
          occasionnelles.
          Puis quelques avantages
          )";
  // The above additional French words ensure the translate bar will appear.
  //
  // TODO(crbug.com/1258185): The current translate testing overrides the
  // result to be Adopted Language: 'fr' (the language the Chrome's
  // translate feature believes the page language to be in). The behavior
  // required here is to only force a translation which should not rely on
  // language detection. The override simply just seeds the translate code
  // so that a translate event occurs in a more testable way.

  NavigateToContentAndWaitForLanguageDetection(kForm);
  ASSERT_EQ("fr", GetLanguageState().current_language());
  ASSERT_NO_FATAL_FAILURE(Translate(true));
  ASSERT_EQ("fr", GetLanguageState().source_language());
  ASSERT_EQ("en", GetLanguageState().current_language());

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));
}

// Test phone fields parse correctly from a given profile.
// The high level key presses execute the following: Select the first text
// field, invoke the autofill popup list, select the first profile within the
// list, and commit to the profile to populate the form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ComparePhoneNumbers) {
  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, u"Bob");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"1234 H St.");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"San Jose");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"95110");
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1-408-555-4567");
  AddTestProfile(browser()->profile(), profile);

  GURL url = embedded_test_server()->GetURL("/autofill/form_phones.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST1"), this));
  EXPECT_EQ("Bob", GetFieldValueById("NAME_FIRST1"));
  EXPECT_EQ("Smith", GetFieldValueById("NAME_LAST1"));
  EXPECT_EQ("1234 H St.", GetFieldValueById("ADDRESS_HOME_LINE1"));
  EXPECT_EQ("San Jose", GetFieldValueById("ADDRESS_HOME_CITY"));
  EXPECT_EQ("CA", GetFieldValueById("ADDRESS_HOME_STATE"));
  EXPECT_EQ("95110", GetFieldValueById("ADDRESS_HOME_ZIP"));
  EXPECT_EQ("14085554567", GetFieldValueById("PHONE_HOME_WHOLE_NUMBER"));

  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST2"), this));
  EXPECT_EQ("Bob", GetFieldValueById("NAME_FIRST2"));
  EXPECT_EQ("Smith", GetFieldValueById("NAME_LAST2"));
  EXPECT_EQ("408", GetFieldValueById("PHONE_HOME_CITY_CODE-1"));
  EXPECT_EQ("5554567", GetFieldValueById("PHONE_HOME_NUMBER"));

  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST3"), this));
  EXPECT_EQ("Bob", GetFieldValueById("NAME_FIRST3"));
  EXPECT_EQ("Smith", GetFieldValueById("NAME_LAST3"));
  EXPECT_EQ("408", GetFieldValueById("PHONE_HOME_CITY_CODE-2"));
  EXPECT_EQ("555", GetFieldValueById("PHONE_HOME_NUMBER_3-1"));
  EXPECT_EQ("4567", GetFieldValueById("PHONE_HOME_NUMBER_4-1"));
  EXPECT_EQ("", GetFieldValueById("PHONE_HOME_EXT-1"));

  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST4"), this));
  EXPECT_EQ("Bob", GetFieldValueById("NAME_FIRST4"));
  EXPECT_EQ("Smith", GetFieldValueById("NAME_LAST4"));
  EXPECT_EQ("1", GetFieldValueById("PHONE_HOME_COUNTRY_CODE-1"));
  EXPECT_EQ("408", GetFieldValueById("PHONE_HOME_CITY_CODE-3"));
  EXPECT_EQ("555", GetFieldValueById("PHONE_HOME_NUMBER_3-2"));
  EXPECT_EQ("4567", GetFieldValueById("PHONE_HOME_NUMBER_4-2"));
  EXPECT_EQ("", GetFieldValueById("PHONE_HOME_EXT-2"));
}

// Test that Autofill does not fill in Company Name if disabled
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, NoAutofillForCompanyName) {
  std::string addr_line1("1234 H St.");
  std::string company_name("Company X");

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, u"Bob");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetRawInfo(EMAIL_ADDRESS, u"bsmith@gmail.com");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16(addr_line1));
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"San Jose");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"95110");
  profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16(company_name));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"408-871-4567");
  AddTestProfile(browser()->profile(), profile);

  GURL url =
      embedded_test_server()->GetURL("/autofill/read_only_field_test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  EXPECT_EQ(addr_line1, GetFieldValueById("address"));
  EXPECT_EQ(company_name, GetFieldValueById("company"));
}

// TODO(https://crbug.com/1279102): Check back if flakiness is fixed now.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       NoAutofillSuggestionForCompanyName) {
  static const char kTestShippingFormWithCompanyString[] = R"(
      An example of a shipping address form.
      <form action="https://www.example.com/" method="POST">
      <label for="firstname">First name:</label>
       <input type="text" id="firstname"><br>
      <label for="lastname">Last name:</label>
       <input type="text" id="lastname"><br>
      <label for="address1">Address line 1:</label>
       <input type="text" id="address1"><br>
      <label for="address2">Address line 2:</label>
       <input type="text" id="address2"><br>
      <label for="city">City:</label>
       <input type="text" id="city"><br>
      <label for="state">State:</label>
       <select id="state">
       <option value="" selected="yes">--</option>
       <option value="CA">California</option>
       <option value="TX">Texas</option>
       </select><br>
      <label for="zip">ZIP code:</label>
       <input type="text" id="zip"><br>
      <label for="country">Country:</label>
       <select id="country">
       <option value="" selected="yes">--</option>
       <option value="CA">Canada</option>
       <option value="US">United States</option>
       </select><br>
      <label for="phone">Phone number:</label>
       <input type="text" id="phone"><br>
      <label for="company">First company:</label>
       <input type="text" id="company"><br>
      </form>
  )";
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormWithCompanyString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByClick(),
                            .execution_target = GetWebContents()}));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
}

// Test that Autofill does not fill in read-only fields.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, NoAutofillForReadOnlyFields) {
  std::string addr_line1("1234 H St.");

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, u"Bob");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetRawInfo(EMAIL_ADDRESS, u"bsmith@gmail.com");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16(addr_line1));
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"San Jose");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"95110");
  profile.SetRawInfo(COMPANY_NAME, u"Company X");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"408-871-4567");
  AddTestProfile(browser()->profile(), profile);

  GURL url =
      embedded_test_server()->GetURL("/autofill/read_only_field_test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  EXPECT_EQ("", GetFieldValueById("email"));
  EXPECT_EQ(addr_line1, GetFieldValueById("address"));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST"), this));

  ASSERT_TRUE(content::ExecJs(GetWebContents(),
                              "document.getElementById('testform').reset()"));

  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST"), this));

  EXPECT_EQ("Milton", GetFieldValueById("NAME_FIRST"));
  EXPECT_EQ("Waddams", GetFieldValueById("NAME_LAST"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("EMAIL_ADDRESS"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("ADDRESS_HOME_LINE1"));
  EXPECT_EQ("Austin", GetFieldValueById("ADDRESS_HOME_CITY"));
  EXPECT_EQ("Texas", GetFieldValueById("ADDRESS_HOME_STATE"));
  EXPECT_EQ("78744", GetFieldValueById("ADDRESS_HOME_ZIP"));
  EXPECT_EQ("United States", GetFieldValueById("ADDRESS_HOME_COUNTRY"));
  EXPECT_EQ("5125551234", GetFieldValueById("PHONE_HOME_WHOLE_NUMBER"));
}

// Test Autofill distinguishes a middle initial in a name.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DistinguishMiddleInitialWithinName) {
  CreateTestProfile();

  GURL url =
      embedded_test_server()->GetURL("/autofill/autofill_middleinit_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST"), this));

  EXPECT_EQ("C.", GetFieldValueById("NAME_MIDDLE"));
}

// Test forms with multiple email addresses are filled properly.
// Entire form should be filled with one user gesture.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       MultipleEmailFilledByOneUserGesture) {
  std::string email("bsmith@gmail.com");

  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, u"Bob");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(email));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"4088714567");
  AddTestProfile(browser()->profile(), profile);

  GURL url = embedded_test_server()->GetURL(
      "/autofill/autofill_confirmemail_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST"), this));

  EXPECT_EQ(email, GetFieldValueById("EMAIL_CONFIRM"));
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

  constexpr int kNumProfiles = 1500;
  for (int i = 0; i < kNumProfiles; i++) {
    AutofillProfile profile;
    std::u16string name(base::NumberToString16(i));
    std::u16string email(name + u"@example.com");
    std::u16string street =
        ASCIIToUTF16(base::NumberToString(base::RandInt(0, 10000)) + " " +
                     streets[base::RandInt(0, streets.size() - 1)]);
    std::u16string city =
        ASCIIToUTF16(cities[base::RandInt(0, cities.size() - 1)]);
    std::u16string zip(base::NumberToString16(base::RandInt(0, 10000)));
    profile.SetRawInfo(NAME_FIRST, name);
    profile.SetRawInfo(EMAIL_ADDRESS, email);
    profile.SetRawInfo(ADDRESS_HOME_LINE1, street);
    profile.SetRawInfo(ADDRESS_HOME_CITY, city);
    profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
    profile.SetRawInfo(ADDRESS_HOME_ZIP, zip);
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
    AddTestProfile(browser()->profile(), profile);
  }

  GURL url = embedded_test_server()->GetURL(
      "/autofill/latency_after_submit_test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST"), this));

  content::LoadStopObserver load_stop_observer(GetWebContents());

  ASSERT_TRUE(content::ExecJs(GetWebContents(),
                              "document.getElementById('testform').submit();"));
  // This will ensure the test didn't hang.
  load_stop_observer.Wait();
}

// Test that Chrome doesn't crash when autocomplete is disabled while the user
// is interacting with the form.  This is a regression test for
// http://crbug.com/160476
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       DisableAutocompleteWhileFilling) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  // When suggestions are shown, disable autocomplete for the active field.
  auto SetAutocompleteOff = [this]() {
    ASSERT_TRUE(content::ExecJs(
        GetWebContents(),
        "document.querySelector('input').autocomplete = 'off';"));
  };

  ASSERT_TRUE(AutofillFlow(
      GetElementById("firstname"), this,
      {.show_method = ShowMethod::ByChar('M'),
       .after_select = base::BindLambdaForTesting(SetAutocompleteOff)}));
}

// Test that a page with 2 forms with no name and id containing fields with no
// name or if get filled properly.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       FillFormAndFieldWithNoNameOrId) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "/autofill/forms_without_identifiers.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto name = ElementExpr("document.forms[1].elements[0]");
  auto email = ElementExpr("document.forms[1].elements[1]");
  ASSERT_TRUE(
      AutofillFlow(name, this, {.show_method = ShowMethod::ByChar('M')}));
  EXPECT_EQ("Milton C. Waddams", GetFieldValue(name));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValue(email));
}

// The following four tests verify that we can autofill forms with multiple
// nameless forms, and repetitive field names and make sure that the dynamic
// refill would not trigger a wrong refill, regardless of the form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       Dynamic_MultipleNoNameForms_BadNames_FourthForm) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/multiple_noname_forms_badnames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_4"), this));
  DoNothingAndWait(base::Seconds(2));  // Wait to for possible refills.
  EXPECT_EQ("", GetFieldValueById("firstname_1"));
  EXPECT_EQ("", GetFieldValueById("lastname_1"));
  EXPECT_EQ("", GetFieldValueById("email_1"));
  EXPECT_EQ("", GetFieldValueById("firstname_2"));
  EXPECT_EQ("", GetFieldValueById("lastname_2"));
  EXPECT_EQ("", GetFieldValueById("email_2"));
  EXPECT_EQ("", GetFieldValueById("firstname_3"));
  EXPECT_EQ("", GetFieldValueById("lastname_3"));
  EXPECT_EQ("", GetFieldValueById("email_3"));
  EXPECT_EQ("Milton", GetFieldValueById("firstname_4"));
  EXPECT_EQ("Waddams", GetFieldValueById("lastname_4"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email_4"));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       Dynamic_MultipleNoNameForms_BadNames_ThirdForm) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/multiple_noname_forms_badnames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_3"), this));
  DoNothingAndWait(base::Seconds(2));  // Wait to for possible refills.
  EXPECT_EQ("", GetFieldValueById("firstname_1"));
  EXPECT_EQ("", GetFieldValueById("lastname_1"));
  EXPECT_EQ("", GetFieldValueById("email_1"));
  EXPECT_EQ("", GetFieldValueById("firstname_2"));
  EXPECT_EQ("", GetFieldValueById("lastname_2"));
  EXPECT_EQ("", GetFieldValueById("email_2"));
  EXPECT_EQ("Milton", GetFieldValueById("firstname_3"));
  EXPECT_EQ("Waddams", GetFieldValueById("lastname_3"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email_3"));
  EXPECT_EQ("", GetFieldValueById("firstname_4"));
  EXPECT_EQ("", GetFieldValueById("lastname_4"));
  EXPECT_EQ("", GetFieldValueById("email_4"));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       Dynamic_MultipleNoNameForms_BadNames_SecondForm) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/multiple_noname_forms_badnames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_2"), this));
  DoNothingAndWait(base::Seconds(2));  // Wait to for possible refills.
  EXPECT_EQ("", GetFieldValueById("firstname_1"));
  EXPECT_EQ("", GetFieldValueById("lastname_1"));
  EXPECT_EQ("", GetFieldValueById("email_1"));
  EXPECT_EQ("Milton", GetFieldValueById("firstname_2"));
  EXPECT_EQ("Waddams", GetFieldValueById("lastname_2"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email_2"));
  EXPECT_EQ("", GetFieldValueById("firstname_3"));
  EXPECT_EQ("", GetFieldValueById("lastname_3"));
  EXPECT_EQ("", GetFieldValueById("email_3"));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       Dynamic_MultipleNoNameForms_BadNames_FirstForm) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/multiple_noname_forms_badnames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_1"), this));
  DoNothingAndWait(base::Seconds(2));  // Wait to for possible refills.
  EXPECT_EQ("Milton", GetFieldValueById("firstname_1"));
  EXPECT_EQ("Waddams", GetFieldValueById("lastname_1"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email_1"));
  EXPECT_EQ("", GetFieldValueById("firstname_2"));
  EXPECT_EQ("", GetFieldValueById("lastname_2"));
  EXPECT_EQ("", GetFieldValueById("email_2"));
  EXPECT_EQ("", GetFieldValueById("firstname_3"));
  EXPECT_EQ("", GetFieldValueById("lastname_3"));
  EXPECT_EQ("", GetFieldValueById("email_3"));
}

// Test that we can Autofill forms where some fields name change during the
// fill.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, FieldsChangeName) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "/autofill/field_changing_name_during_fill.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that credit card autofill works.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestBase, FillLocalCreditCard) {
  CreateTestCreditCart();
  GURL url = https_server()->GetURL("a.com",
                                    "/autofill/autofill_creditcard_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("CREDIT_CARD_NAME_FULL"), this));
  EXPECT_EQ("Milton Waddams", GetFieldValueById("CREDIT_CARD_NAME_FULL"));
  EXPECT_EQ("4111111111111111", GetFieldValueById("CREDIT_CARD_NUMBER"));
  EXPECT_EQ("09", GetFieldValueById("CREDIT_CARD_EXP_MONTH"));
  EXPECT_EQ("2999", GetFieldValueById("CREDIT_CARD_EXP_4_DIGIT_YEAR"));
}

// Test that we do not fill formless non-checkout forms when we enable the
// formless form restrictions.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestBase, NoAutocomplete) {
  CreateTestProfile();
  GURL url =
      embedded_test_server()->GetURL("/autofill/formless_no_autocomplete.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  // If only some form fields are tagged with autocomplete types, then the
  // number of input elements will not match the number of fields when autofill
  // tries to preview or fill.
  ASSERT_TRUE(WaitAndExpectUniqueSample(&histogram_tester(),
                                        kNumElementsMatchesNumFields, true, 2));

  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that we do not fill formless non-checkout forms when we enable the
// formless form restrictions. This test differs from the NoAutocomplete
// version of the the test in that at least one of the fields has an
// autocomplete attribute, so autofill will always be aware of the existence
// of the form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestBase, SomeAutocomplete) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "/autofill/formless_some_autocomplete.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  // If only some form fields are tagged with autocomplete types, then the
  // number of input elements will not match the number of fields when autofill
  // tries to preview or fill.
  ASSERT_TRUE(WaitAndExpectUniqueSample(&histogram_tester(),
                                        kNumElementsMatchesNumFields, true, 2));

  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that we do not fill formless non-checkout forms when we enable the
// formless form restrictions.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestBase, AllAutocomplete) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "/autofill/formless_all_autocomplete.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  // If all form fields are tagged with autocomplete types, we make them all
  // available to be filled.
  ASSERT_TRUE(WaitAndExpectUniqueSample(&histogram_tester(),
                                        kNumElementsMatchesNumFields, true, 2));

  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that an 'onchange' event is not fired when a <selectlist> preview
// suggestion is shown or hidden.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       NoEventFiredWhenExitingSelectListPreview) {
  // It is hard to test that an event will not happen in the future, but we
  // assume that applying similar operations on two elements in sequence results
  // in a consistent order of events triggered by the operations. So the test
  // strategy here is to first trigger a preview on `state` select, and then
  // select an element on `other`.

  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "/autofill/form_selectlist_preview_no_onchange.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Show autofill preview.
  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.do_accept = false}));

  // Hide autofill preview.
  content::RenderWidgetHost* render_widget_host =
      GetWebContents()->GetRenderWidgetHostView()->GetRenderWidgetHost();
  ASSERT_TRUE(SendKeyToPopupAndWait(ui::DomKey::ESCAPE,
                                    {ObservedUiEvents::kSuggestionsHidden},
                                    render_widget_host));
  ASSERT_FALSE(IsPopupShown());

  // Select element on `other` and wait for `onchange` event.
  ValueWaiter onchange_waiter =
      ListenForValueChange("other", absl::nullopt, GetWebContents());
  ASSERT_TRUE(FocusField(GetElementById("other"), GetWebContents()));
  EXPECT_EQ("First", GetFieldValueById("other"));
  FillElementWithValue("other", "Second");
  ASSERT_TRUE(std::move(onchange_waiter).Wait());

  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "other_changed;"));
  EXPECT_EQ(false, content::EvalJs(GetWebContents(), "state_changed;"));
}

// An extension of the test fixture for tests with site isolation.
class AutofillInteractiveIsolationTest : public AutofillInteractiveTestBase {
 protected:
  AutofillInteractiveIsolationTest() = default;
  ~AutofillInteractiveIsolationTest() override = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillInteractiveTestBase::SetUpCommandLine(command_line);
    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }
};

enum class FrameType { kIFrame, kFencedFrame };

class AutofillInteractiveFencedFrameTest
    : public AutofillInteractiveIsolationTest,
      public ::testing::WithParamInterface<FrameType> {
 protected:
  AutofillInteractiveFencedFrameTest() {
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (GetParam() != FrameType::kIFrame) {
      enabled.push_back({blink::features::kBrowsingTopics, {}});
      enabled.push_back({blink::features::kFencedFramesAPIChanges, {}});
      scoped_feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
      fenced_frame_test_helper_ =
          std::make_unique<content::test::FencedFrameTestHelper>();
    }
  }
  ~AutofillInteractiveFencedFrameTest() override = default;

  content::RenderFrameHost* primary_main_frame_host() {
    return GetWebContents()->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* LoadSubFrame(std::string relative_url) {
    GURL frame_url = https_server()->GetURL(
        "b.com", (GetParam() == FrameType::kIFrame ? "" : "/fenced_frames") +
                     relative_url);
    switch (GetParam()) {
      case FrameType::kIFrame: {
        EXPECT_TRUE(content::NavigateIframeToURL(GetWebContents(), "crossFrame",
                                                 frame_url));
        // TODO(crbug.com/1323334) Use AutofillManager::OnFormParsed instead of
        // DoNothingAndWait.
        // Wait to make sure the cross-frame form is parsed.
        DoNothingAndWait(base::Seconds(2));
        content::RenderFrameHost* cross_frame =
            RenderFrameHostForName(GetWebContents(), "crossFrame");
        return cross_frame;
      }
      case FrameType::kFencedFrame: {
        // Creates a <fencedframe> element in the renderer.
        content::RenderFrameHost* cross_frame =
            fenced_frame_test_helper_->CreateFencedFrame(
                primary_main_frame_host(), frame_url);
        // TODO(crbug.com/1323334) Use AutofillManager::OnFormParsed instead of
        // DoNothingAndWait.
        // Wait to make sure the cross-frame form is parsed.
        DoNothingAndWait(base::Seconds(2));
        return cross_frame;
      }
    }
    NOTREACHED();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::test::FencedFrameTestHelper>
      fenced_frame_test_helper_;
};

INSTANTIATE_TEST_SUITE_P(AutofillInteractiveTest,
                         AutofillInteractiveFencedFrameTest,
                         ::testing::Values(FrameType::kFencedFrame,
                                           FrameType::kIFrame));

// TODO(https://crbug.com/1175735): Check back if flakiness is fixed now.
IN_PROC_BROWSER_TEST_P(AutofillInteractiveFencedFrameTest,
                       SimpleCrossSiteFill) {
  test_delegate()->SetIgnoreBackToBackMessages(
      ObservedUiEvents::kPreviewFormData, true);
  CreateTestProfile();

  // Main frame is on a.com, iframe/fenced frame is on b.com.
  GURL url =
      https_server()->GetURL("a.com", "/autofill/cross_origin_iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* cross_frame_host =
      LoadSubFrame("/autofill/autofill_test_form.html");
  ASSERT_TRUE(cross_frame_host);

  ContentAutofillDriver* cross_driver =
      ContentAutofillDriverFactory::FromWebContents(GetWebContents())
          ->DriverForFrame(cross_frame_host);
  ASSERT_TRUE(cross_driver);
  // Let |test_delegate()| also observe autofill events in the iframe.
  test_delegate()->Observe(cross_driver->GetAutofillManager());

  ASSERT_TRUE(AutofillFlow(GetElementById("NAME_FIRST"), this,
                           {.execution_target = cross_frame_host}));
  EXPECT_EQ("Milton",
            GetFieldValue(GetElementById("NAME_FIRST"), cross_frame_host));
}

// This test verifies that credit card (payment card list) popup works when the
// form is inside an OOPIF/Fenced Frame.
IN_PROC_BROWSER_TEST_P(AutofillInteractiveFencedFrameTest,
                       CrossSitePaymentForms) {
  CreateTestCreditCart();
  // Main frame is on a.com, iframe/fenced frame is on b.com.
  GURL url =
      https_server()->GetURL("a.com", "/autofill/cross_origin_iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* cross_frame_host =
      LoadSubFrame("/autofill/autofill_creditcard_form.html");
  ASSERT_TRUE(cross_frame_host);

  ContentAutofillDriver* cross_driver =
      ContentAutofillDriverFactory::FromWebContents(GetWebContents())
          ->DriverForFrame(cross_frame_host);
  ASSERT_TRUE(cross_driver);
  // Let |test_delegate()| also observe autofill events in the iframe.
  test_delegate()->Observe(cross_driver->GetAutofillManager());

  auto Wait = [this]() { DoNothingAndWait(base::Seconds(2)); };
  ASSERT_TRUE(AutofillFlow(GetElementById("CREDIT_CARD_NUMBER"), this,
                           {.after_focus = base::BindLambdaForTesting(Wait),
                            .execution_target = cross_frame_host}));
}

// Tests that deleting the subframe that has opened the Autofill popup closes
// the popup.
// TODO(https://crbug.com/1175735): Check back if flakiness is fixed now.
IN_PROC_BROWSER_TEST_P(AutofillInteractiveFencedFrameTest,
                       DeletingFrameClosesPopup) {
  CreateTestProfile();

  // Main frame is on a.com, fenced frame is on b.com.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL("a.com", "/autofill/cross_origin_iframe.html")));

  content::RenderFrameHost* cross_frame_host =
      LoadSubFrame("/autofill/autofill_test_form.html");
  ASSERT_TRUE(cross_frame_host);

  // We need the fencedframe element to have id set to a known value
  if (GetParam() != FrameType::kIFrame) {
    ASSERT_TRUE(content::ExecJs(
        GetWebContents(),
        "document.getElementsByTagName('fencedframe')[0].id = 'crossFF';"));
  }

  ContentAutofillDriver* cross_driver =
      ContentAutofillDriverFactory::FromWebContents(GetWebContents())
          ->DriverForFrame(cross_frame_host);
  ASSERT_TRUE(cross_driver);
  // Let |test_delegate()| also observe autofill events in the iframe.
  test_delegate()->Observe(cross_driver->GetAutofillManager());

  // Open the Autofill popup but do not accept the suggestion yet. Deleting the
  // subframe should close the popup.
  ASSERT_TRUE(
      AutofillFlow(GetElementById("NAME_FIRST"), this,
                   {.do_accept = false, .execution_target = cross_frame_host}));
  // Do not accept the suggestion yet, to keep the pop-up shown.
  EXPECT_TRUE(IsPopupShown());

  // Delete the iframe/fenced frame.
  std::string script_delete = base::StringPrintf(
      "document.body.removeChild(document.getElementById('%s'))",
      GetParam() == FrameType::kIFrame ? "crossFrame" : "crossFF");
  ASSERT_TRUE(content::ExecJs(GetWebContents(), script_delete));

  EXPECT_FALSE(IsPopupShown());
}

// Tests that when changing the tab while the popup is open, closes the popup.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ChangingTabClosesPopup) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));
  // Open the Autofill popup but do not accept the suggestion yet. Selecting
  // another tab should close the popup.
  content::WebContents* original_tab = GetWebContents();
  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.do_accept = false}));
  EXPECT_TRUE(IsPopupShown());
  AddBlankTabAndShow(browser());
  ASSERT_NE(original_tab, GetWebContents());
  chrome::CloseWebContents(browser(), GetWebContents(),
                           /*add_to_history=*/false);
  ASSERT_EQ(original_tab, GetWebContents());
  EXPECT_FALSE(IsPopupShown());
}

// Test fixture for refill behavior.
//
// BrowserAutofillManager only executes a refill if it happens within the time
// delta `kLimitBeforeRefill` of the original refill. On slow bots, this timeout
// may cause flakiness. Therefore, this fixture mocks test clocks, which shall
// be advanced when waiting for a refill after AutofillFlow():
// - advance by a delta less than `kLimitBeforeRefill` to simulate that a
//   natural delay between fill and refill;
// - advance by a delta greater than `kLimitBeforeRefill` to simulate that an
//   event happens too late to actually trigger a refill.
class AutofillInteractiveTestDynamicForm : public AutofillInteractiveTest {
 public:
  ValueWaiter ListenForRefill(
      const std::string& id,
      absl::optional<std::string> unblock_variable = "refill") {
    return ListenForValueChange(id, unblock_variable, GetWebContents());
  }

  // Refills only happen within `kLimitBeforeRefill` second of the initial fill.
  // Slow bots may exceed this limit and thus cause flakiness.
  static constexpr base::TimeDelta kLessThanLimitBeforeRefill =
      kLimitBeforeRefill / 10;

  void AdvanceClock(base::TimeDelta delta) {
    clock_.Advance(delta);
    tick_clock_.Advance(delta);
  }

 protected:
  TestAutofillClock clock_{AutofillClock::Now()};
  TestAutofillTickClock tick_clock_{AutofillTickClock::NowTicks()};
};

// Test that we can Autofill dynamically generated forms.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill) {
  CreateTestProfile();
  GURL url =
      embedded_test_server()->GetURL("a.com", "/autofill/dynamic_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname_form1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname_form1"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address_form1"));
  EXPECT_EQ("TX", GetFieldValueById("state_form1"));
  EXPECT_EQ("Austin", GetFieldValueById("city_form1"));
  EXPECT_EQ("Initech", GetFieldValueById("company_form1"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email_form1"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone_form1"));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       TwoDynamicChangingFormsFill) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL("a.com",
                                            "/autofill/two_dynamic_forms.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname_form1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_form1"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname_form1"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address_form1"));
  EXPECT_EQ("TX", GetFieldValueById("state_form1"));
  EXPECT_EQ("Austin", GetFieldValueById("city_form1"));
  EXPECT_EQ("Initech", GetFieldValueById("company_form1"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email_form1"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone_form1"));

  refill = ListenForRefill("firstname_form2");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_form2"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname_form2"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address_form2"));
  EXPECT_EQ("TX", GetFieldValueById("state_form2"));
  EXPECT_EQ("Austin", GetFieldValueById("city_form2"));
  EXPECT_EQ("Initech", GetFieldValueById("company_form2"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email_form2"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone_form2"));
}

// Test that forms that dynamically change a second time do not get filled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_SecondChange) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/double_dynamic_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname_form2");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_FALSE(std::move(refill).Wait());

  // Make sure the new form was not filled.
  EXPECT_EQ("", GetFieldValueById("firstname_form2"));
  EXPECT_EQ("", GetFieldValueById("address_form2"));
  EXPECT_EQ("CA", GetFieldValueById("state_form2"));  // Default value.
  EXPECT_EQ("", GetFieldValueById("city_form2"));
  EXPECT_EQ("", GetFieldValueById("company_form2"));
  EXPECT_EQ("", GetFieldValueById("email_form2"));
  EXPECT_EQ("", GetFieldValueById("phone_form2"));
}

// Test that forms that dynamically change after a second do not get filled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_AfterDelay) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_after_delay.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname_form1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLimitBeforeRefill + base::Milliseconds(1));
  ASSERT_FALSE(std::move(refill).Wait());

  // Make sure that the new form was not filled.
  EXPECT_EQ("", GetFieldValueById("firstname_form1"));
  EXPECT_EQ("", GetFieldValueById("address_form1"));
  EXPECT_EQ("CA", GetFieldValueById("state_form1"));  // Default value.
  EXPECT_EQ("", GetFieldValueById("city_form1"));
  EXPECT_EQ("", GetFieldValueById("company_form1"));
  EXPECT_EQ("", GetFieldValueById("email_form1"));
  EXPECT_EQ("", GetFieldValueById("phone_form1"));
}

// Test that only field of a type group that was filled initially get refilled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_AddsNewFieldTypeGroups) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_new_field_types.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname_form1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // The fields present in the initial fill should be filled.
  EXPECT_EQ("Milton", GetFieldValueById("firstname_form1"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address_form1"));
  EXPECT_EQ("TX", GetFieldValueById("state_form1"));
  EXPECT_EQ("Austin", GetFieldValueById("city_form1"));
  // Fields from group that were not present in the initial fill should not be
  // filled
  EXPECT_EQ("", GetFieldValueById("company_form1"));
  // Fields that were present but hidden in the initial fill should not be
  // filled.
  EXPECT_EQ("", GetFieldValueById("email_form1"));
  // The phone should be filled even if it's a different format than the initial
  // fill.
  EXPECT_EQ("5125551234", GetFieldValueById("phone_form1"));
}

// Test that we can autofill forms that dynamically change select fields to text
// fields by changing the visibilities.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicFormFill_SelectToText) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_select_to_text.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1"));
  EXPECT_EQ("Texas", GetFieldValueById("state_us"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that we can autofill forms that dynamically change the visibility of a
// field after it's autofilled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicFormFill_VisibilitySwitch) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_visibility_switch.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1"));
  // Both fields must be filled after a refill.
  EXPECT_EQ("Texas", GetFieldValueById("state_first"));
  EXPECT_EQ("Texas", GetFieldValueById("state_second"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on.
// TODO(crbug.com/1481004): Re-enable this test
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DynamicFormFill_FirstElementDisappears \
  DISABLED_DynamicFormFill_FirstElementDisappears
#else
#define MAYBE_DynamicFormFill_FirstElementDisappears \
  DynamicFormFill_FirstElementDisappears
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       MAYBE_DynamicFormFill_FirstElementDisappears) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_element_invalid.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("address1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname2"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though the form has no name.
// TODO(crbug.com/1481004): Re-enable this test
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DynamicFormFill_FirstElementDisappearsNoNameForm \
  DISABLED_DynamicFormFill_FirstElementDisappearsNoNameForm
#else
#define MAYBE_DynamicFormFill_FirstElementDisappearsNoNameForm \
  DynamicFormFill_FirstElementDisappearsNoNameForm
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       MAYBE_DynamicFormFill_FirstElementDisappearsNoNameForm) {
  CreateTestProfile();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_element_invalid_noname_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("address1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname2"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though there are multiple forms with identical
// names.
IN_PROC_BROWSER_TEST_F(
    AutofillInteractiveTestDynamicForm,
    DynamicFormFill_FirstElementDisappearsMultipleBadNameForms) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com",
      "/autofill/dynamic_form_element_invalid_multiple_badname_forms.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("address1_7");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_5"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the second form was filled correctly, and the first form was left
  // unfilled.
  EXPECT_EQ("", GetFieldValueById("firstname_1"));
  EXPECT_EQ("", GetFieldValueById("firstname_2"));
  EXPECT_EQ("", GetFieldValueById("address1_3"));
  EXPECT_EQ("CA", GetFieldValueById("country_4"));  // default
  EXPECT_EQ("Milton", GetFieldValueById("firstname_6"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1_7"));
  EXPECT_EQ("US", GetFieldValueById("country_8"));
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though there are multiple forms with identical
// names.
// TODO(crbug.com/1481004): Re-enable this test
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DynamicFormFill_FirstElementDisappearsBadnameUnowned \
  DISABLED_DynamicFormFill_FirstElementDisappearsBadnameUnowned
#else
#define MAYBE_DynamicFormFill_FirstElementDisappearsBadnameUnowned \
  DynamicFormFill_FirstElementDisappearsBadnameUnowned
#endif
IN_PROC_BROWSER_TEST_F(
    AutofillInteractiveTestDynamicForm,
    MAYBE_DynamicFormFill_FirstElementDisappearsBadnameUnowned) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_element_invalid_unowned_badnames.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("address1_7");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_5"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the second form was filled correctly, and the first form was left
  // unfilled.
  EXPECT_EQ("", GetFieldValueById("firstname_1"));
  EXPECT_EQ("", GetFieldValueById("firstname_2"));
  EXPECT_EQ("", GetFieldValueById("address1_3"));
  EXPECT_EQ("CA", GetFieldValueById("country_4"));  // default
  EXPECT_EQ("Milton", GetFieldValueById("firstname_6"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1_7"));
  EXPECT_EQ("US", GetFieldValueById("country_8"));
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though there are multiple forms with no name.
IN_PROC_BROWSER_TEST_F(
    AutofillInteractiveTestDynamicForm,
    DynamicFormFill_FirstElementDisappearsMultipleNoNameForms) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com",
      "/autofill/dynamic_form_element_invalid_multiple_noname_forms.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("address1_7");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname_5"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the second form was filled correctly, and the first form was left
  // unfilled.
  EXPECT_EQ("", GetFieldValueById("firstname_1"));
  EXPECT_EQ("", GetFieldValueById("firstname_2"));
  EXPECT_EQ("", GetFieldValueById("address1_3"));
  EXPECT_EQ("CA", GetFieldValueById("country_4"));  // default
  EXPECT_EQ("Milton", GetFieldValueById("firstname_6"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1_7"));
  EXPECT_EQ("US", GetFieldValueById("country_8"));
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though the elements are unowned.
// TODO(crbug.com/1481004): Re-enable this test
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DynamicFormFill_FirstElementDisappearsUnowned \
  DISABLED_DynamicFormFill_FirstElementDisappearsUnowned
#else
#define MAYBE_DynamicFormFill_FirstElementDisappearsUnowned \
  DynamicFormFill_FirstElementDisappearsUnowned
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       MAYBE_DynamicFormFill_FirstElementDisappearsUnowned) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_element_invalid_unowned.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("address1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname2"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that credit card fields are re-filled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_AlsoForCreditCard) {
  CreateTestCreditCart();
  GURL url = https_server()->GetURL("a.com",
                                    "/autofill/dynamic_form_credit_card.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("cc-name");
  ASSERT_TRUE(AutofillFlow(GetElementById("cc-name"), this,
                           {.show_method = ShowMethod::ByChar('M')}));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait(base::Seconds(10)));

  EXPECT_EQ("Milton Waddams", GetFieldValueById("cc-name"));
  EXPECT_EQ("4111111111111111", GetFieldValueById("cc-num"));
  EXPECT_EQ("09", GetFieldValueById("cc-exp-month"));
  EXPECT_EQ("2999", GetFieldValueById("cc-exp-year"));
  EXPECT_EQ("", GetFieldValueById("cc-csc"));
}

void DoDynamicChangingFormFill_SelectUpdated(
    AutofillInteractiveTestDynamicForm* test,
    net::EmbeddedTestServer* test_server,
    bool should_test_selectlist,
    bool should_test_async_update) {
  test->CreateTestProfile();
  GURL url = test_server->GetURL(
      "a.com",
      base::StringPrintf(
          ("/autofill/dynamic_form_select_or_selectlist_options_change.html"
           "?is_selectlist=%s&is_async=%s"),
          should_test_selectlist ? "true" : "false",
          should_test_async_update ? "true" : "false"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(test->browser(), url));

  // Check that the test page correctly parsed the 'is_selectlist' GET parameter
  // by checking type of the inserted field.
  auto has_n_controls_of_type = [](FormControlType control_type,
                                   size_t expected_number,
                                   const FormStructure& form) {
    size_t num_found = 0u;
    for (const std::unique_ptr<AutofillField>& field : form.fields()) {
      if (field->form_control_type == control_type) {
        ++num_found;
      }
    }
    return num_found == expected_number;
  };
  ASSERT_TRUE(WaitForMatchingForm(
      test->GetBrowserAutofillManager(),
      should_test_selectlist
          ? base::BindRepeating(has_n_controls_of_type,
                                FormControlType::kSelectList, 1)
          : base::BindRepeating(has_n_controls_of_type,
                                FormControlType::kSelectOne, 2)));

  ValueWaiter refill = test->ListenForRefill("state");
  // Trigger first fill.
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), test));
  // Wait till the first onchange event fired on the 'state' field after the
  // <option>s in the 'state' field have been updated.
  test->AdvanceClock(
      AutofillInteractiveTestDynamicForm::kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Check that the page correctly parsed the 'is_async' GET parameter.
  ASSERT_EQ(should_test_async_update, test->GetFieldCheckedById("is_async"));

  // Make sure the new form was filled correctly.
  EXPECT_EQ(kDefaultAddressValues.first_name,
            test->GetFieldValueById("firstname"));
  EXPECT_EQ(kDefaultAddressValues.address1,
            test->GetFieldValueById("address1"));
  EXPECT_EQ(kDefaultAddressValues.state_short,
            test->GetFieldValueById("state"));
  EXPECT_EQ(kDefaultAddressValues.city, test->GetFieldValueById("city"));
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed.
// TODO(crbug.com/1481004) Flaky on win-asan.
#if defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_WIN)
#define MAYBE_DynamicChangingFormFill_SelectUpdated \
    DISABLED_DynamicChangingFormFill_SelectUpdated
#else
#define MAYBE_DynamicChangingFormFill_SelectUpdated \
    DynamicChangingFormFill_SelectUpdated
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       MAYBE_DynamicChangingFormFill_SelectUpdated) {
  DoDynamicChangingFormFill_SelectUpdated(this, embedded_test_server(),
                                          /*should_test_selectlist=*/false,
                                          /*should_test_async_update=*/false);
}

// Test that we can Autofill dynamically changing selectlists that have options
// added and removed.
// TODO(crbug.com/1481004) Flaky on win-asan.
#if defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_WIN)
#define MAYBE_DynamicChangingFormFill_SelectListUpdated \
    DISABLED_DynamicChangingFormFill_SelectListUpdated
#else
#define MAYBE_DynamicChangingFormFill_SelectListUpdated \
    DynamicChangingFormFill_SelectListUpdated
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       MAYBE_DynamicChangingFormFill_SelectListUpdated) {
  DoDynamicChangingFormFill_SelectUpdated(this, embedded_test_server(),
                                          /*should_test_selectlist=*/true,
                                          /*should_test_async_update=*/false);
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed, when the updating occurs asynchronously.
// TODO(crbug.com/1481004) Flaky on win-asan.
#if defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_WIN)
#define MAYBE_DynamicChangingFormFill_SelectUpdatedAsync \
    DISABLED_DynamicChangingFormFill_SelectUpdatedAsync
#else
#define MAYBE_DynamicChangingFormFill_SelectUpdatedAsync \
    DynamicChangingFormFill_SelectUpdatedAsync
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       MAYBE_DynamicChangingFormFill_SelectUpdatedAsync) {
  DoDynamicChangingFormFill_SelectUpdated(this, embedded_test_server(),
                                          /*should_test_selectlist=*/false,
                                          /*should_test_async_update=*/true);
}

// Test that we can Autofill dynamically changing selectlists that have options
// added and removed, when the updating occurs asynchronously.
// TODO(crbug.com/1481004) Flaky on win-asan.
#if defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_WIN)
#define MAYBE_DynamicChangingFormFill_SelectListUpdatedAsync \
    DISABLED_DynamicChangingFormFill_SelectListUpdatedAsync
#else
#define MAYBE_DynamicChangingFormFill_SelectListUpdatedAsync \
    DynamicChangingFormFill_SelectListUpdatedAsync
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       MAYBE_DynamicChangingFormFill_SelectListUpdatedAsync) {
  DoDynamicChangingFormFill_SelectUpdated(this, embedded_test_server(),
                                          /*should_test_selectlist=*/true,
                                          /*should_test_async_update=*/true);
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed only once.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_DoubleSelectUpdated) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_double_select_options_change.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill1 = ListenForRefill("address1", "refill1");
  ValueWaiter refill2 = ListenForRefill("firstname", "refill2");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill1).Wait());
  ASSERT_FALSE(std::move(refill2).Wait());

  // Upon the first fill, JS resets the address1 field, which triggers a refill.
  // Upon the refill, JS resets the T
  EXPECT_EQ("", GetFieldValueById(
                    "firstname"));  // That field value was reset dynamically.
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1"));
  EXPECT_EQ("CA", GetFieldValueById("state"));  // The <select>'s default value.
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that we can Autofill dynamically generated forms with no name if the
// NameForAutofill of the first field matches.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_FormWithoutName) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_form_no_name.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname_form1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname_form1"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address_form1"));
  EXPECT_EQ("TX", GetFieldValueById("state_form1"));
  EXPECT_EQ("Austin", GetFieldValueById("city_form1"));
  EXPECT_EQ("Initech", GetFieldValueById("company_form1"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email_form1"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone_form1"));
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed for forms with no names if the NameForAutofill of the first
// field matches.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_SelectUpdated_FormWithoutName) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com",
      "/autofill/dynamic_form_with_no_name_select_options_change.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Test that we can Autofill dynamically generated synthetic forms if the
// NameForAutofill of the first field matches.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_SyntheticForm) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_synthetic_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname_syntheticform1");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname_syntheticform1"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address_syntheticform1"));
  EXPECT_EQ("TX", GetFieldValueById("state_syntheticform1"));
  EXPECT_EQ("Austin", GetFieldValueById("city_syntheticform1"));
  EXPECT_EQ("Initech", GetFieldValueById("company_syntheticform1"));
  EXPECT_EQ("red.swingline@initech.com",
            GetFieldValueById("email_syntheticform1"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone_syntheticform1"));
}

// Test that we can Autofill dynamically synthetic forms when the select options
// change if the NameForAutofill of the first field matches
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_SelectUpdated_SyntheticForm) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/dynamic_synthetic_form_select_options_change.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter refill = ListenForRefill("firstname");
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(refill).Wait());

  // Make sure the new form was filled correctly.
  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address1"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("5125551234", GetFieldValueById("phone"));
}

// Some websites have JavaScript handlers that mess with the input of the user
// and autofill. A common problem is that the date "09/2999" gets reformatted
// into "09 / 20" instead of "09 / 99".
// In these tests, the following steps will happen:
// 1) Autofill recognizes an expiration date field with maxlength=7, will infer
//    that it is supposed to fill 09/2999 and will fill that value.
// 2) The website sees the content 09/2999 and reformats it to 09 / 29 because
//    this is what websites do sometimes.
// 3) The AutofillAgent recognizes that it failed to fill 09/2999 and fills
//    09 / 99 instead.
// 4) The promise waits to see 09 / 99 and resolved.
// Flaky on Mac https://crbug.com/1462103.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       FillCardOnReformattingForm) {
  CreateTestCreditCart();
  GURL url = https_server()->GetURL(
      "a.com", "/autofill/autofill_creditcard_form_with_date_formatter.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ValueWaiter reformat_waiter =
      ListenForValueChange("CREDIT_CARD_EXP_DATE", "unblock", GetWebContents());
  ASSERT_TRUE(AutofillFlow(GetElementById("CREDIT_CARD_NAME_FULL"), this));
  AdvanceClock(kLessThanLimitBeforeRefill);
  ASSERT_TRUE(std::move(reformat_waiter).Wait());
  EXPECT_EQ("09 / 99", GetFieldValue(GetElementById("CREDIT_CARD_EXP_DATE")));

  // The timestamp from BrowserAutofillManager::OnDidFillAutofillFormData()
  // comes from the renderer process and thus from an actual clock. Since this
  // interaction timestamp must be before the submission timestamp, we advance
  // the browser by a lot.
  AdvanceClock(base::Minutes(10));

  // Since votes are emitted and quality metrics are recorded asynchronously, we
  // need to explicitly wait for the pending votes. Since voting is scheduled on
  // submission, we first need to wait for the submission (otherwise, there are
  // no pending to vote for).
  //
  // Additionally, we wait for a navigation because that's when the key metrics
  // are emitted.
  content::LoadStopObserver load_stop_observer(GetWebContents());
  BrowserAutofillManager* autofill_manager = GetBrowserAutofillManager();
  TestAutofillManagerWaiter submission_waiter(
      *autofill_manager, {AutofillManagerEvent::kFormSubmitted});
  ASSERT_TRUE(content::ExecJs(GetWebContents(),
                              "document.getElementById('testform').submit();"));
  ASSERT_TRUE(submission_waiter.Wait(1));
  ASSERT_TRUE(test_api(*autofill_manager).FlushPendingVotes());
  load_stop_observer.Wait();

  // Short hand for ExpectBucketCount:
  auto expect_count = [&](base::StringPiece name,
                          base::HistogramBase::Sample sample,
                          base::HistogramBase::Count expected_count) {
    histogram_tester().ExpectBucketCount(name, sample, expected_count);
  };
  expect_count("Autofill.KeyMetrics.FillingReadiness.CreditCard", 1, 1);
  expect_count("Autofill.KeyMetrics.FillingAcceptance.CreditCard", 1, 1);
  expect_count("Autofill.KeyMetrics.FillingCorrectness.CreditCard", 1, 1);
  expect_count("Autofill.KeyMetrics.FillingAssistance.CreditCard", 1, 1);
  // Ensure that refills don't count as edits.
  expect_count("Autofill.NumberOfEditedAutofilledFieldsAtSubmission", 0, 1);
  expect_count("Autofill.PerfectFilling.CreditCards", 1, 1);
  // Bucket 0 = edited, 1 = accepted; 3 samples for 3 fields.
  expect_count("Autofill.EditedAutofilledFieldAtSubmission.Aggregate", 0, 0);
  expect_count("Autofill.EditedAutofilledFieldAtSubmission.Aggregate", 1, 3);
}

// Shadow DOM tests consist of two cases:
// - Case 0: the <form> is in the main DOM;
// - Case 1: the <form> is in a shadow DOM.
class AutofillInteractiveTestShadowDom
    : public AutofillInteractiveTest,
      public ::testing::WithParamInterface<size_t> {
 public:
  size_t case_num() const { return GetParam(); }

  // Replaces "$1" in `str` with the `case_num()`.
  std::string WithCaseNum(base::StringPiece str) const {
    return base::ReplaceStringPlaceholders(
        str, {base::NumberToString(case_num())}, nullptr);
  }

  ElementExpr JsElement(base::StringPiece js_expr) {
    return ElementExpr(WithCaseNum(js_expr));
  }

  content::EvalJsResult Js(base::StringPiece js_code) {
    return content::EvalJs(GetWebContents(), WithCaseNum(js_code));
  }
};

INSTANTIATE_TEST_SUITE_P(AutofillInteractiveTest,
                         AutofillInteractiveTestShadowDom,
                         ::testing::Values(0, 1));

// Tests that in a shadow-DOM-transcending form, Autofill detects labels
// *outside* of the field's shadow DOM.
IN_PROC_BROWSER_TEST_P(AutofillInteractiveTestShadowDom,
                       LabelInHostingDomOfField) {
  CreateTestProfile();
  GURL url =
      embedded_test_server()->GetURL("a.com", "/autofill/shadowdom.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(JsElement("getNameElement($1)"), this));
  EXPECT_EQ("Milton C. Waddams", Js("getName($1)"));
  EXPECT_EQ("4120 Freidrich Lane", Js("getAddress($1)"));
  EXPECT_EQ("Austin", Js("getCity($1)"));
  EXPECT_EQ("TX", Js("getState($1)"));
  EXPECT_EQ("78744", Js("getZip($1)"));
}

// Tests that in a shadow-DOM-transcending form, Autofill detects labels
// *inside* of the field's shadow DOM.
IN_PROC_BROWSER_TEST_P(AutofillInteractiveTestShadowDom,
                       LabelInSameShadowDomAsField) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/shadowdom-no-inference.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(JsElement("getNameElement($1)"), this));
  EXPECT_EQ("Milton C. Waddams", Js("getName($1)"));
  EXPECT_EQ("4120 Freidrich Lane", Js("getAddress($1)"));
  EXPECT_EQ("TX", Js("getState($1)"));
}

// ChromeVox is only available on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(ENABLE_EXTENSIONS)

class AutofillInteractiveTestChromeVox : public AutofillInteractiveTestBase {
 public:
  AutofillInteractiveTestChromeVox() = default;
  ~AutofillInteractiveTestChromeVox() override = default;

  void TearDownOnMainThread() override {
    // Unload the ChromeVox extension so the browser doesn't try to respond to
    // in-flight requests during test shutdown. https://crbug.com/923090
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(false);
    AutomationManagerAura::GetInstance()->Disable();
    AutofillInteractiveTestBase::TearDownOnMainThread();
  }

  void EnableChromeVox() {
    // Test setup.
    // Enable ChromeVox, disable earcons and wait for key mappings to be
    // fetched.
    ASSERT_FALSE(ash::AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
    // TODO(accessibility): fix console error/warnings and instantiate
    // |console_observer_| here.

    // Load ChromeVox and block until it's fully loaded.
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
    sm_.ExpectSpeechPattern("*");
    sm_.Call([this]() { DisableEarcons(); });
  }

  void DisableEarcons() {
    // Playing earcons from within a test is not only annoying if you're
    // running the test locally, but seems to cause crashes
    // (http://crbug.com/396507). Work around this by just telling
    // ChromeVox to not ever play earcons (prerecorded sound effects).
    extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        browser()->profile(), extension_misc::kChromeVoxExtensionId,
        "ChromeVox.earcons.playEarcon = function() {};");
  }

  ash::test::SpeechMonitor sm_;
};

// Ensure that autofill suggestions are properly read out via ChromeVox.
// This is a regressions test for crbug.com/1208913.
// TODO(https://crbug.com/1294266): Flaky on ChromeOS
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestNotificationOfAutofillDropdown \
  DISABLED_TestNotificationOfAutofillDropdown
#else
#define MAYBE_TestNotificationOfAutofillDropdown \
  TestNotificationOfAutofillDropdown
#endif
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestChromeVox,
                       MAYBE_TestNotificationOfAutofillDropdown) {
  CreateTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  EnableChromeVox();
  content::EnableAccessibilityForWebContents(web_contents());

  // The following contains a sequence of calls to
  // sm_.ExpectSpeechPattern() and test_delegate()->Wait(). It is essential
  // to first flush the expected speech patterns, otherwise the two functions
  // start incompatible RunLoops.
  sm_.ExpectSpeechPattern("Web Content");
  sm_.Call([this]() {
    content::WaitForAccessibilityTreeToContainNodeWithName(web_contents(),
                                                           "First name:");
    web_contents()->Focus();
    test_delegate()->SetExpectations({ObservedUiEvents::kSuggestionsShown});
    ASSERT_TRUE(FocusField(GetElementById("firstname"), GetWebContents()));
  });
  sm_.ExpectSpeechPattern("First name:");
  sm_.ExpectSpeechPattern("Edit text");
  sm_.ExpectSpeechPattern("Region");
  // Wait for suggestions popup to show up. This needs to happen before we
  // simulate the cursor down key press.
  sm_.Call([this]() { ASSERT_TRUE(test_delegate()->Wait()); });
  sm_.Call([this]() {
    test_delegate()->SetExpectations({ObservedUiEvents::kPreviewFormData});
    ASSERT_TRUE(
        ui_controls::SendKeyPress(browser()->window()->GetNativeWindow(),
                                  ui::VKEY_DOWN, false, false, false, false));
  });
  sm_.ExpectSpeechPattern("Autofill menu opened");
  sm_.ExpectSpeechPattern("Milton 4120 Freidrich Lane");
  sm_.ExpectSpeechPattern("List item");
  sm_.ExpectSpeechPattern("1 of 2");
  sm_.Call([this]() { ASSERT_TRUE(test_delegate()->Wait()); });
  sm_.Replay();
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(ENABLE_EXTENSIONS)

// These tests are disabled on LaCros because <select> elements don't listen
// to typed characters the same way as other platforms. Sending the characters
// 'W', 'A' while the state selector is focused does not trigger a selection
// of the entry "WA".
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_AutofillInteractiveFormSubmissionTest \
  DISABLED_AutofillInteractiveFormSubmissionTest
#else
#define MAYBE_AutofillInteractiveFormSubmissionTest \
  AutofillInteractiveFormSubmissionTest
#endif
class MAYBE_AutofillInteractiveFormSubmissionTest
    : public AutofillInteractiveTestBase {
 public:
  class MockAutofillManager : public BrowserAutofillManager {
   public:
    MockAutofillManager(ContentAutofillDriver* driver, AutofillClient* client)
        : BrowserAutofillManager(driver, client, "en-US") {}
    MOCK_METHOD(void,
                OnFormSubmittedImpl,
                (const FormData&, bool, mojom::SubmissionSource),
                (override));
  };

  MockAutofillManager* autofill_manager() {
    return autofill_manager(GetWebContents()->GetPrimaryMainFrame());
  }

  MockAutofillManager* autofill_manager(content::RenderFrameHost* rfh) {
    return autofill_manager_injector_[rfh];
  }

  void SetUpOnMainThread() override {
    AutofillInteractiveTestBase::SetUpOnMainThread();

    SetUpServer();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));
    ASSERT_TRUE(WaitForMatchingForm(
        autofill_manager(), base::BindRepeating([](const FormStructure& form) {
          return form.active_field_count() == 5;
        })));
  }

  void SetUpServer() {
    SetTestUrlResponse(R"(
        <html><body>
        <form id='shipping' method='POST' action='/success.html'>
        Name: <input type='text' id='name'><br>
        Address: <input type='text' id='address'><br>
        City: <input type='text' id='city'><br>
        ZIP: <input type='text' id='zip'><br>
        State: <select id='state'>
          <option value='CA'>CA</option>
          <option value='TX'>TX</option>
          <option value='WA'>WA</option>
        </select><br>
        </form>
    )");
    SetResponseForUrlPath("/success.html", "<html><body>Happy times!");
    SetResponseForUrlPath("/xhr", "<foo>Happy times!</foo>");
  }

  void EnterValues() {
    // Normally we would enter the "US state" last, but we don't have a
    // kSelectElementDidChange event, yet. Use multi-arg version of
    // EnterValues() to wait until the last field was reported to the autofill
    // manager.
    EnterValues(
        {{"name", "Sarah"}, {"state", "WA"}, {"address", "123 Main Road"}},
        /*num_modified_textfields=*/2u);
  }

  void EnterValues(const std::vector<FieldValue>& values,
                   size_t num_modified_textfields) {
    TestAutofillManagerWaiter waiter(
        *autofill_manager(), {AutofillManagerEvent::kTextFieldDidChange});
    for (const FieldValue& value : values) {
      ASSERT_TRUE(EnterTextIntoField(GetElementById(value.id), value.value,
                                     this, GetWebContents()));
    }
    ASSERT_TRUE(waiter.Wait(num_modified_textfields));
  }

  [[nodiscard]] static auto HasExpectedValues() {
    struct NameValue {
      std::u16string name;
      std::u16string value;
    };
    std::vector<NameValue> expected = {{u"name", u"Sarah"},
                                       {u"address", u"123 Main Road"},
                                       {u"city", u""},
                                       {u"zip", u""},
                                       {u"state", u"WA"}};
    return FieldsAre(Map(expected, [](const NameValue& nv) {
      return AllOf(Field("name", &FormFieldData::name, nv.name),
                   Field("value", &FormFieldData::value, nv.value));
    }));
  }

  struct NameValueUserInput {
    std::u16string name;
    std::u16string value;
    std::u16string user_input;
  };
  [[nodiscard]] static auto HasNameValueUserInput(
      const NameValueUserInput& nvu) {
    return AllOf(
        Field("name", &FormFieldData::name, nvu.name),
        Field("value", &FormFieldData::value, nvu.value),
        Field("user_input", &FormFieldData::user_input, nvu.user_input));
  }

  void ExecuteScript(const std::string& script) {
    ASSERT_TRUE(content::ExecJs(GetWebContents(), script));
  }

 private:
  TestAutofillManagerInjector<MockAutofillManager> autofill_manager_injector_;
};

// Tests that user-triggered submission triggers a submission event in
// BrowserAutofillManager.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       Submission) {
  EnterValues();

  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(*autofill_manager(),
              OnFormSubmittedImpl(HasExpectedValues(),
                                  /*known_success=*/false,
                                  mojom::SubmissionSource::FORM_SUBMISSION))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));
  ExecuteScript("document.getElementById('shipping').submit();");
  run_loop.Run();
}

// Tests that non-link-click, renderer-initiated navigation triggers a
// submission event in BrowserAutofillManager.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       ProbableSubmission) {
  EnterValues();

  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(HasExpectedValues(),
                          /*known_success=*/false,
                          mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));
  // Add a delay before navigating away to avoid race conditions. This is
  // appropriate since we're faking user interaction here.
  ExecuteScript(
      "setTimeout(() => { window.location.assign('/success.html'); }, 50);");
  run_loop.Run();
}

// Tests that a same document navigation can trigger a form submission.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       SameDocumentNavigation) {
  EnterValues();

  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(HasExpectedValues(),
                          /*known_success=*/true,
                          mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));

  // Simulate form submission.
  ExecuteScript(
      R"(
      // Same document navigation:
      document.getElementById('shipping').style.display = 'none';
      const url = new URL(window.location);
      url.searchParams.set('foo', 'bar');
      window.history.pushState({}, '', url);

      // Hide form, which is the trigger for the submission event.
      document.getElementById('shipping').style.display = 'none';
      )");
  run_loop.Run();
}

// Tests that an XHR request can indicate a form submission.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       XhrSucceededAndHideForm) {
  EnterValues();

  base::RunLoop run_loop;

  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(*autofill_manager(),
              OnFormSubmittedImpl(HasExpectedValues(),
                                  /*known_success=*/true,
                                  mojom::SubmissionSource::XHR_SUCCEEDED))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));

  // Simulate form submission.
  ExecuteScript(
      R"(
      // SubmissionSource::XHR_SUCCEEDED is triggered if an XHR is observed
      // after the form has been made invisible.
      document.getElementById('shipping').style.display = 'none';

      const xhr = new XMLHttpRequest();
      xhr.open('GET', '/xhr', true);
      xhr.send(null);
      )");
  run_loop.Run();
}

// Tests that an XHR request can indicate a form submission - even if the form
// is deleted from the DOM.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       XhrSucceededAndDeleteForm) {
  EnterValues();

  base::RunLoop run_loop;

  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(*autofill_manager(),
              OnFormSubmittedImpl(HasExpectedValues(),
                                  /*known_success=*/true,
                                  mojom::SubmissionSource::XHR_SUCCEEDED))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));

  // Simulate form submission.
  ExecuteScript(
      R"(
      // SubmissionSource::XHR_SUCCEEDED is triggered if an XHR is observed
      // after the form has been deleted.
      const form = document.getElementById('shipping');
      form.remove();

      const xhr = new XMLHttpRequest();
      xhr.open('GET', '/xhr', true);
      xhr.send(null);
      )");
  run_loop.Run();
}

// Tests that a DOM mutation after an XHR can indicate a form submission.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       DomMutationAfterXhr) {
  EnterValues();

  base::RunLoop run_loop;

  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(HasExpectedValues(),
                          /*known_success=*/true,
                          mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));

  // Simulate form submission.
  ExecuteScript(
      R"(
      const xhr = new XMLHttpRequest();
      xhr.open('GET', '/xhr', true);
      xhr.onload = () => {
        // SubmissionSource::DOM_MUTATION_AFTER_XHR is triggered if a form
        // is hidden an XHR was observed.
        // The DOM modification has to happen asynchronously. Otherwise this
        // is reported as an XHR_SUCCEEDED event.
        setTimeout(() => {
            document.getElementById('shipping').style.display = 'none';
          }, 50);
      }
      xhr.send(null);
      )");
  run_loop.Run();
}

// Tests that FormFieldData::user_input has the text that the user typed into
// the field. This is needed in order to show the save-card dialog when the
// page replaces the <input> value with '***'.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       RememberUserInput) {
  const std::vector<NameValueUserInput> kExpectedSubmittedValues{
      {u"name", u"JS Modified Name", u"Sarah"},
      {u"address", u"JS Modified Address", u"123 Main Road"},
      {u"city", u"", u""},
      {u"zip", u"", u""},
      {u"state", u"WA", u""}};  // user_input is not set for <select>.

  EnterValues();
  ExecuteScript("document.getElementById('name').value = 'JS Modified Name';");
  ExecuteScript(
      "document.getElementById('address').value = 'JS Modified Address';");

  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(
          FieldsAre(Map(kExpectedSubmittedValues, HasNameValueUserInput)),
          /*known_success=*/false, mojom::SubmissionSource::FORM_SUBMISSION))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));
  ExecuteScript("document.getElementById('shipping').submit();");
  run_loop.Run();
}

// Tests scenario where in sequence:
// 1) The user types into a form
// 2) The form is cleared via JavaScript
// 3) The user autofills the form
// 4) The user submits the form
// That FormFieldData::user_input is empty and does not contain stale data that
// the user typed into the form.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       TreatAutofillAsUserInput) {
  CreateTestProfile();

  EnterValues({{"address", "User Entered Address"}},
              /*num_modified_textfields=*/1u);
  ExecuteScript("document.getElementById('address').value = '';");

  ASSERT_TRUE(AutofillFlow(GetElementById("name"), this,
                           {.show_method = ShowMethod::ByChar('M')}));
  const std::vector<FieldValue> kExpectedAddress{
      {"name", kDefaultAddressValues.full_name},
      {"address", kDefaultAddressValues.address1},
      {"city", kDefaultAddressValues.city},
      {"zip", kDefaultAddressValues.zip},
      {"state", kDefaultAddressValues.state_short}};
  EXPECT_THAT(GetFormValues(), ValuesAre(kExpectedAddress));

  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(
          FieldsAre(Map(kExpectedAddress,
                        [](const FieldValue& fv) {
                          return HasNameValueUserInput(
                              {base::UTF8ToUTF16(fv.id),
                               base::UTF8ToUTF16(fv.value), u""});
                        })),
          /*known_success=*/false, mojom::SubmissionSource::FORM_SUBMISSION))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));
  ExecuteScript("document.getElementById('shipping').submit();");
  run_loop.Run();
}

// MAYBE_AutofillInteractiveFormSubmissionTest subclass which disables
// features::kAutofillUndo
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_AutofillInteractiveFormSubmissionClearFormTest \
  DISABLED_AutofillInteractiveFormSubmissionClearFormTest
#else
#define MAYBE_AutofillInteractiveFormSubmissionClearFormTest \
  AutofillInteractiveFormSubmissionClearFormTest
#endif
class MAYBE_AutofillInteractiveFormSubmissionClearFormTest
    : public MAYBE_AutofillInteractiveFormSubmissionTest {
 public:
   MAYBE_AutofillInteractiveFormSubmissionClearFormTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kAutofillUndo);
   }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests scenario where in sequence:
// 1) The user autofills the form
// 2) The user clears the form via the context menu
// 3) The user submits the form
// That FormFieldData::user_input is empty and does not contain stale data.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionClearFormTest,
                       ClearFormClearsUserInput) {
  CreateTestProfile();

  // Autofill
  ASSERT_TRUE(AutofillFlow(GetElementById("name"), this,
                           {.show_method = ShowMethod::ByChar('M')}));
  const std::vector<FieldValue> kAutofilledAddress{
      {"name", kDefaultAddressValues.full_name},
      {"address", kDefaultAddressValues.address1},
      {"city", kDefaultAddressValues.city},
      {"zip", kDefaultAddressValues.zip},
      {"state", kDefaultAddressValues.state_short}};
  EXPECT_THAT(GetFormValues(), ValuesAre(kAutofilledAddress));

  // Clear form.
  ASSERT_TRUE(AutofillFlow(GetElementById("name"), this, {.target_index = 1}));
  const std::vector<FieldValue> kClearedAddress{{"name", ""},
                                                {"address", ""},
                                                {"city", ""},
                                                {"zip", ""},
                                                {"state", "CA"}};
  EXPECT_THAT(GetFormValues(), ValuesAre(kClearedAddress));

  std::vector<NameValueUserInput> kSubmittedValues = {{u"name", u"", u""},
                                                      {u"address", u"", u""},
                                                      {u"city", u"", u""},
                                                      {u"zip", u"", u""},
                                                      {u"state", u"CA", u""}};

  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(
          FieldsAre(Map(kSubmittedValues, HasNameValueUserInput)),
          /*known_success=*/false, mojom::SubmissionSource::FORM_SUBMISSION))
      .Times(1)
      .WillRepeatedly(InvokeClosure(run_loop.QuitClosure()));
  ExecuteScript("document.getElementById('shipping').submit();");
  run_loop.Run();
}

}  // namespace autofill
