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
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
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
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

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

// Version of `kTestShippingFormString` which uses <selectmenu> instead of
// <select>.
std::string GenerateTestShippingFormWithSelectMenu() {
  std::string out = kTestShippingFormString;
  RE2::GlobalReplace(&out, "<(/?)select", "<\\1selectmenu");
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

// Represents a JavaScript expression that evaluates to a HTMLElement.
using ElementExpr = base::StrongAlias<struct ElementExprTag, std::string>;

ElementExpr GetElementById(const std::string& id) {
  return ElementExpr(
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

// Returns the center point of a DOM element.
gfx::Point GetCenter(const ElementExpr& e,
                     content::ToRenderFrameHost execution_target) {
  std::string x_script = base::StringPrintf(
      R"( const bounds = (%s).getBoundingClientRect();
          Math.floor(bounds.left + bounds.width / 2))",
      e->c_str());
  std::string y_script = base::StringPrintf(
      R"( const bounds = (%s).getBoundingClientRect();
          Math.floor(bounds.top + bounds.height / 2)
      )",
      e->c_str());
  int x = content::EvalJs(execution_target, x_script).ExtractInt();
  int y = content::EvalJs(execution_target, y_script).ExtractInt();
  return gfx::Point(x, y);
}

// Triggers a JavaScript event like 'focus' and waits for the event to happen.
[[nodiscard]] AssertionResult TriggerAndWaitForEvent(
    const ElementExpr& e,
    const std::string& event_name,
    content::ToRenderFrameHost execution_target) {
  std::string script = base::StringPrintf(
      R"( new Promise(resolve => {
            if (document.readyState === 'complete') {
              function handler(e) {
                e.target.removeEventListener(e.type, arguments.callee);
                resolve(true);
              }
              const target = %s;
              target.addEventListener('%s', handler);
              target.%s();
            } else {
              resolve(false);
            }
          });
          )",
      e->c_str(), event_name.c_str(), event_name.c_str());
  content::EvalJsResult result = content::EvalJs(execution_target, script);
  if (!result.error.empty()) {
    return AssertionFailure() << __func__ << "(): " << result.error;
  } else if (false == result) {
    return AssertionFailure()
           << __func__ << "(): couldn't trigger " << event_name << " on " << *e;
  } else {
    return AssertionSuccess();
  }
}

// True iff `e` is the deepest active element in the given frame.
//
// "Deepest" refers to the shadow DOM: if an <input> in the shadow DOM is
// focused, then this <input> and the shadow host are active elements, but
// IsFocusedField() only returns true for the <input>.
bool IsFocusedField(const ElementExpr& e,
                    content::ToRenderFrameHost execution_target) {
  std::string script = base::StringPrintf(
      "const e = (%s); e === e.getRootNode().activeElement", e->c_str());
  return true == content::EvalJs(execution_target, script);
}

// Unfocuses the currently focused field.
[[nodiscard]] AssertionResult BlurFocusedField(
    content::ToRenderFrameHost execution_target) {
  std::string script = R"(
    if (document.activeElement !== null)
      document.activeElement.blur();
  )";
  return content::ExecJs(execution_target, script);
}

// A helper function for focusing a field in AutofillFlow().
// Consider using AutofillFlow() instead.
[[nodiscard]] AssertionResult FocusField(
    const ElementExpr& e,
    content::ToRenderFrameHost execution_target) {
  if (IsFocusedField(e, execution_target)) {
    AssertionResult r = BlurFocusedField(execution_target);
    if (!r)
      return r;
  }
  return TriggerAndWaitForEvent(e, "focus", execution_target);
}

// Types the characters of `value` after focusing field `e`.
[[nodiscard]] AssertionResult EnterTextIntoField(
    const ElementExpr& e,
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

// Executes `EnterTextIntoField()` for a series of fields.
[[nodiscard]] AssertionResult EnterTextsIntoFields(
    std::vector<std::pair<ElementExpr, std::string>> values,
    AutofillUiTest* test,
    content::ToRenderFrameHost execution_target) {
  for (const auto& [element, value] : values) {
    AssertionResult a =
        EnterTextIntoField(element, value, test, execution_target);
    if (!a) {
      return AssertionFailure() << __func__ << "(): " << a;
    }
  }
  return AssertionSuccess();
}

// The different ways of triggering the Autofill dropdown.
//
// A difference in their behaviour is that (only) ByArrow() opens implicitly
// selects the top-most suggestion.
struct ShowMethod {
  constexpr static ShowMethod ByArrow() { return {.arrow = true}; }
  constexpr static ShowMethod ByClick() { return {.click = true}; }
  constexpr static ShowMethod ByChar(char c) { return {.character = c}; }

  bool selects_first_suggestion() { return arrow; }

  // Exactly one of the members should be evaluate to `true`.
  const bool arrow = false;
  const char character = '\0';
  const bool click = false;
};

// We choose the timeout empirically. 250 ms are not enough; tests become flaky:
// in ShowAutofillPopup(), the preview triggered by an "arrow down" sometimes
// only arrives after >250 ms and thus arrives during the DoNothingAndWait(),
// which causes a crash.
constexpr base::TimeDelta kAutofillFlowDefaultTimeout = base::Seconds(2);

struct ShowAutofillPopupParams {
  ShowMethod show_method = ShowMethod::ByArrow();
  int num_profile_suggestions = 1;
  size_t max_tries = 5;
  base::TimeDelta timeout = kAutofillFlowDefaultTimeout;
  absl::optional<content::ToRenderFrameHost> execution_target = {};
};

// A helper function for showing the popup in AutofillFlow().
// Consider using AutofillFlow() instead.
[[nodiscard]] AssertionResult ShowAutofillPopup(const ElementExpr& e,
                                                AutofillUiTest* test,
                                                ShowAutofillPopupParams p) {
  constexpr auto kSuggest = ObservedUiEvents::kSuggestionsShown;
  constexpr auto kPreview = ObservedUiEvents::kPreviewFormData;

  content::ToRenderFrameHost execution_target =
      p.execution_target.value_or(test->GetWebContents());
  content::RenderFrameHost* rfh = execution_target.render_frame_host();
  content::RenderWidgetHostView* view = rfh->GetView();
  content::RenderWidgetHost* widget = view->GetRenderWidgetHost();

  auto ArrowDown = [&](std::list<ObservedUiEvents> exp) {
    constexpr auto kDown = ui::DomKey::ARROW_DOWN;
    if (base::Contains(exp, ObservedUiEvents::kSuggestionsShown)) {
      return test->SendKeyToPageAndWait(kDown, std::move(exp), p.timeout);
    } else {
      return test->SendKeyToPopupAndWait(kDown, std::move(exp), widget,
                                         p.timeout);
    }
  };
  auto Backspace = [&]() {
    return test->SendKeyToPageAndWait(ui::DomKey::BACKSPACE, {}, p.timeout);
  };
  auto Char = [&](const std::string& code, std::list<ObservedUiEvents> exp) {
    ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(code);
    ui::DomKey dom_key;
    ui::KeyboardCode keyboard_code;
    CHECK(ui::DomCodeToUsLayoutDomKey(dom_code, ui::EF_SHIFT_DOWN, &dom_key,
                                      &keyboard_code));
    return test->SendKeyToPageAndWait(dom_key, dom_code, keyboard_code,
                                      std::move(exp), p.timeout);
  };
  auto Click = [&](std::list<ObservedUiEvents> exp) {
    gfx::Point point = view->TransformPointToRootCoordSpace(GetCenter(e, rfh));
    test->test_delegate()->SetExpectations(
        {ObservedUiEvents::kSuggestionsShown}, p.timeout);
    content::SimulateMouseClickAt(test->GetWebContents(), 0,
                                  blink::WebMouseEvent::Button::kLeft, point);
    return test->test_delegate()->Wait();
  };

  // It seems that due to race conditions with Blink's layouting
  // (crbug.com/1175735#c9), the below focus events are sometimes too early:
  // Autofill closes the popup right away because it is outside of the content
  // area. To work around this, we attempt to bring up the Autofill popup
  // multiple times, with some delay.
  AssertionResult a = AssertionFailure()
                      << __func__ << "(): with " << p.num_profile_suggestions
                      << " profile suggestions";
  bool field_was_focused_initially = IsFocusedField(e, rfh);
  for (size_t i = 1; i <= p.max_tries; ++i) {
    a = a << "Iteration " << i << "/" << p.max_tries << ". ";
    // A Translate bubble may overlap with the Autofill popup, which causes
    // flakiness. See crbug.com/1175735#c10.
    // Also, the address-save prompts and others may overlap with the Autofill
    // popup. So we preemptively close all bubbles, which however is not
    // reliable on Windows.
    translate::test_utils::CloseCurrentBubble(test->browser());
    TryToCloseAllPrompts(test->GetWebContents());
    if (i > 1) {
      test->DoNothingAndWaitAndIgnoreEvents(p.timeout);
      if (field_was_focused_initially) {
        // The Autofill popup may have opened due to a severely delayed event on
        // a slow bot. To reset the popup, we re-focus the field.
        a << "Trying to re-focus the field. ";
        if (AssertionResult b = BlurFocusedField(rfh); !b)
          a = a << b;
        if (AssertionResult b = FocusField(e, rfh); !b)
          a = a << b;
      }
    }

    bool has_preview = 0 < p.num_profile_suggestions;
    if (p.show_method.arrow) {
      // Press arrow down to open the popup and select first suggestion.
      // Depending on the platform, this requires one or two arrow-downs.
      if (!IsFocusedField(e, rfh))
        return a << "Field " << *e << " must be focused. ";
      if (!ShouldAutoselectFirstSuggestionOnArrowDown()) {
        if (AssertionResult b = ArrowDown({kSuggest}); !b) {
          a << "Cannot trigger suggestions by first arrow: " << b;
          continue;
        }
        if (AssertionResult b =
                has_preview ? ArrowDown({kPreview}) : ArrowDown({});
            !b) {
          a << "Cannot select first suggestion by second arrow: " << b;
          continue;
        }
      } else if (AssertionResult b = has_preview
                                         ? ArrowDown({kSuggest, kPreview})
                                         : ArrowDown({kSuggest});
                 !b) {
        a << "Cannot trigger and select first suggestion by arrow: " << b;
        continue;
      }
    } else if (p.show_method.character) {
      // Enter character to open the popup, but do not select an option.
      // If necessary, delete past iterations character first.
      if (!IsFocusedField(e, rfh))
        return a << "Field " << *e << " must be focused. ";
      if (i > 1) {
        if (AssertionResult b = Backspace(); !b) {
          a << "Cannot undo past iteration's key: " << b;
        }
      }
      std::string code = std::string("Key") + p.show_method.character;
      if (AssertionResult b = Char(code, {kSuggest}); !b) {
        a << "Cannot trigger suggestions by key: " << b;
        continue;
      }
    } else if (p.show_method.click) {
      // Click item to open the popup, but do not select an option.
      if (AssertionResult b = Click({kSuggest}); !b) {
        a << "Cannot trigger and select first suggestion by click: " << b;
        continue;
      }
    }
    return AssertionSuccess();
  }
  return a << "Couldn't show Autofill suggestions on " << *e << ". ";
}

struct AutofillSuggestionParams {
  int num_profile_suggestions = 1;
  int current_index = 0;
  int target_index = 0;
  base::TimeDelta timeout = kAutofillFlowDefaultTimeout;
  absl::optional<content::ToRenderFrameHost> execution_target = {};
};

// A helper function for selecting a suggestion in AutofillFlow().
// Consider using AutofillFlow() instead.
[[nodiscard]] AssertionResult SelectAutofillSuggestion(
    const ElementExpr& e,
    AutofillUiTest* test,
    AutofillSuggestionParams p) {
  content::RenderWidgetHost* widget =
      p.execution_target.value_or(test->GetWebContents())
          .render_frame_host()
          ->GetView()
          ->GetRenderWidgetHost();

  constexpr auto kPreview = ObservedUiEvents::kPreviewFormData;

  auto ArrowDown = [&](std::list<ObservedUiEvents> exp) {
    return test->SendKeyToPopupAndWait(ui::DomKey::ARROW_DOWN, std::move(exp),
                                       widget, p.timeout);
  };

  for (int i = p.current_index + 1; i <= p.target_index; ++i) {
    bool has_preview = i < p.num_profile_suggestions;
    if (!(has_preview ? ArrowDown({kPreview}) : ArrowDown({}))) {
      return AssertionFailure()
             << __func__ << "(): Couldn't go to " << i << "th suggestion with"
             << (has_preview ? "" : "out") << " preview";
    }
  }
  return AssertionSuccess();
}

// A helper function for accepting a suggestion in AutofillFlow().
// Consider using AutofillFlow() instead.
[[nodiscard]] AssertionResult AcceptAutofillSuggestion(
    const ElementExpr& e,
    AutofillUiTest* test,
    AutofillSuggestionParams p) {
  content::RenderWidgetHost* widget =
      p.execution_target.value_or(test->GetWebContents())
          .render_frame_host()
          ->GetView()
          ->GetRenderWidgetHost();

  // If `kAutofillPopupUseThresholdForKeyboardAndMobileAccept` is enabled,
  // then all attempts to accept Autofill suggestions using keyboard "ENTER"
  // keystrokes will be ignored for the first 500ms after the popup is first
  // shown. This overrides this threshold.
  if (base::WeakPtr<AutofillPopupControllerImpl> controller =
          ChromeAutofillClient::FromWebContentsForTesting(
              test->GetWebContents())
              ->popup_controller_for_testing()) {
    controller->DisableThresholdForTesting(true);
  }

  constexpr auto kSuggestionsHidden = ObservedUiEvents::kSuggestionsHidden;
  constexpr auto kFill = ObservedUiEvents::kFormDataFilled;

  auto Enter = [&](std::list<ObservedUiEvents> exp) {
    return test->SendKeyToPopupAndWait(ui::DomKey::ENTER, std::move(exp),
                                       widget);
  };

  bool has_fill = p.target_index < p.num_profile_suggestions;
  if (AssertionResult a = SelectAutofillSuggestion(e, test, p); !a)
    return a;
  if (!(has_fill ? Enter({kSuggestionsHidden, kFill})
                 : Enter({kSuggestionsHidden}))) {
    return AssertionFailure()
           << __func__ << "(): Couldn't accept to " << p.target_index
           << "th suggestion with" << (has_fill ? "" : "out") << " fill";
  }
  return AssertionSuccess();
}

// An Autofill consists of four stages:
// 1. focusing the field,
// 2. showing the Autofill popup,
// 3. selecting the desired suggestion,
// 4. accepting the selected suggestion.
//
// To reduce flakiness when in Stage 2, this does multiple attempts.
// Depending on the `show_method`, showing may already select the first
// suggestion; see ShowMethod for details.
//
// Selecting a profile suggestion (address or credit card) also triggers
// preview. By contrast, "Clear" and "Manage" do not cause a preview. The
// Autofill flow expects a preview for (only) the indices less than
// `num_profile_suggestions`. The selected `target_index` may be greater or
// equal to `num_profile_suggestions` to select "Clear" or "Manager".
//
// A callback can be set to be executed after each stage. Again note that
// `show_method` may select the first suggestion.
//
// The default `execution_target` is the main frame.
struct AutofillFlowParams {
  bool do_focus = true;
  bool do_show = true;
  bool do_select = true;
  bool do_accept = true;
  ShowMethod show_method = ShowMethod::ByArrow();
  int num_profile_suggestions = 1;
  int target_index = 0;
  base::RepeatingClosure after_focus = {};
  base::RepeatingClosure after_show = {};
  base::RepeatingClosure after_select = {};
  base::RepeatingClosure after_accept = {};
  size_t max_show_tries = 5;
  base::TimeDelta timeout = kAutofillFlowDefaultTimeout;
  absl::optional<content::ToRenderFrameHost> execution_target = {};
};

[[nodiscard]] AssertionResult AutofillFlow(const ElementExpr& e,
                                           AutofillUiTest* test,
                                           AutofillFlowParams p = {}) {
  content::ToRenderFrameHost execution_target =
      p.execution_target.value_or(test->GetWebContents());

  if (p.do_focus) {
    AssertionResult a = FocusField(e, execution_target);
    if (!a)
      return a;
    if (p.after_focus)
      p.after_focus.Run();
  }

  if (p.do_show) {
    AssertionResult a =
        ShowAutofillPopup(e, test,
                          {.show_method = p.show_method,
                           .num_profile_suggestions = p.num_profile_suggestions,
                           .max_tries = p.max_show_tries,
                           .timeout = p.timeout,
                           .execution_target = execution_target});
    if (!a)
      return a;
    if (p.after_show)
      p.after_show.Run();
  }

  if (p.do_select) {
    AssertionResult a = SelectAutofillSuggestion(
        e, test,
        {.num_profile_suggestions = p.num_profile_suggestions,
         .current_index = p.show_method.selects_first_suggestion() ? 0 : -1,
         .target_index = p.target_index,
         .timeout = p.timeout,
         .execution_target = execution_target});
    if (!a)
      return a;
    if (p.after_select)
      p.after_select.Run();
  }

  if (p.do_accept) {
    AssertionResult a = AcceptAutofillSuggestion(
        e, test,
        {.num_profile_suggestions = p.num_profile_suggestions,
         .current_index = p.target_index,
         .target_index = p.target_index,
         .timeout = p.timeout,
         .execution_target = execution_target});
    if (!a)
      return a;
    if (p.after_accept)
      p.after_accept.Run();
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
  const char* address1 = "4120 Freidrich Lane";
  const char* address2 = "Basement";
  const char* city = "Austin";
  const char* state_short = "TX";
  const char* state = "Texas";
  const char* zip = "78744";
  const char* country = "US";
  const char* phone = "15125551234";
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

// Matches a container of FieldValues if the `i`th actual FieldValue::value
// matches the `i`th `expected` FieldValue::value.
// As a sanity check, also requires that the `i`th actual FieldValue::id
// starts with the `i`th `expected` FieldValue::id.
[[nodiscard]] auto ValuesAre(const std::vector<FieldValue>& expected) {
  auto FieldEq = [](const FieldValue& expected) {
    return ::testing::AllOf(
        ::testing::Field(&FieldValue::id, ::testing::StartsWith(expected.id)),
        ::testing::Field(&FieldValue::value, ::testing::Eq(expected.value)));
  };
  std::vector<decltype(FieldEq(expected[0]))> matchers;
  for (const FieldValue& field : expected)
    matchers.push_back(FieldEq(field));
  return ::testing::UnorderedElementsAreArray(matchers);
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

// Matcher for a FormData which checks that the submitted fields correspond
// to the name/value pairs in `expected`.
auto SubmittedValuesAre(
    const std::map<std::u16string, std::u16string>& expected) {
  auto get_submitted_values = [](const FormData& form) {
    std::map<std::u16string, std::u16string> result;
    for (const auto& field : form.fields) {
      result[field.name] = field.value;
    }
    return result;
  };
  return ResultOf("get_submitted_values", get_submitted_values,
                  ::testing::ContainerEq(expected));
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
        {blink::features::kAutofillShadowDOM},
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
  // key press event callback. It is necessary to have this sinkbecause if no
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
      : feature_list_(features::kAutofillEnableSelectMenu) {}
  ~AutofillInteractiveTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillInteractiveTestBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        translate::switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
    command_line->AppendSwitchASCII("enable-blink-features",
                                    "HTMLSelectMenuElement");
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

// AutofillInteractiveTest subclass which disables autofilling <selectmenu>.
class AutofillInteractiveDisableAutofillSelectMenuTest
    : public AutofillInteractiveTest {
 protected:
  AutofillInteractiveDisableAutofillSelectMenuTest() {
    feature_list_.InitAndDisableFeature(features::kAutofillEnableSelectMenu);
  }
  ~AutofillInteractiveDisableAutofillSelectMenuTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the <selectmenu> is not filled if the <selectmenu> autofilling
// feature is disabled.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveDisableAutofillSelectMenuTest,
                       DisableSelectMenuAutofilling) {
  const char kFormWithSelectMenuString[] = R"(
    <!-- Disable extra network request for /favicon.ico -->
    <link rel="icon" href="data:,">
    <form action="https://www.example.com/" method="POST" id="shipping">
      <label for="firstname">First name:</label>
      <input type="text" id="firstname" autocomplete="given-name"><br>
      <label for="state">State:</label>
      <selectmenu id="state" autocomplete="address-level1">
        <option value="" selected="yes">--</option>
        <option value="CA">California</option>
        <option value="TX">Texas</option>
      </selectmenu>
    </form>
    )";

  CreateTestProfile();
  SetTestUrlResponse(kFormWithSelectMenuString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  EXPECT_THAT(GetFormValues(),
              ValuesAre({{"firstname", kDefaultAddressValues.first_name},
                         {"state", ""}}));
}

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, BasicClear) {
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

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ClearTwoSection) {
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

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));
  EXPECT_THAT(GetFormValues(),
              ValuesAre(MergeValue(kDefaultAddress, {"city", "Montreal"})));
}

void DoModifySelectFieldAndFill(AutofillInteractiveTest* test,
                                bool should_test_selectmenu) {
  test->CreateTestProfile();
  test->SetTestUrlResponse(should_test_selectmenu
                               ? GenerateTestShippingFormWithSelectMenu()
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
  DoModifySelectFieldAndFill(this, /*should_test_selectmenu=*/false);
}

// Test that autofill doesn't refill a <selectmenu> field initially modified by
// the user.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, ModifySelectMenuFieldAndFill) {
  DoModifySelectFieldAndFill(this, /*should_test_selectmenu=*/true);
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
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
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

// Test that multiple autofillings work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
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

// Test that multiple autofillings work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
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

// Test that multiple autofillings work.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       FillThenFillSomeWithAnotherProfileThenClear) {
  CreateTestProfile();
  CreateSecondTestProfile();
  SetTestUrlResponse(kTestShippingFormString);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestUrl()));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.show_method = ShowMethod::ByChar('M'),
                            .after_select = ExpectValues(MergeValue(
                                kEmptyAddress, {"firstname", "M"}))}));
  EXPECT_THAT(GetFormValues(), ValuesAre(kDefaultAddress));

  // Delete some fields.
  ASSERT_TRUE(FocusField(GetElementById("city"), GetWebContents()));
  DeleteElementValue(GetElementById("city"));
  ASSERT_TRUE(AutofillFlow(
      GetElementById("address1"), this,
      {.target_index = 1, .after_focus = base::BindLambdaForTesting([&]() {
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
  std::string orginalcolor = GetBackgroundColor(GetElementById("firstname"));
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this,
                           {.num_profile_suggestions = 0, .target_index = 1}));
  EXPECT_EQ("Bob", GetFieldValueById("firstname"));
  EXPECT_EQ(GetBackgroundColor(GetElementById("firstname")), orginalcolor);
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

  EXPECT_THAT(input_element_events, testing::ElementsAre("input", "change"));

  EXPECT_EQ(2,
            content::EvalJs(GetWebContents(), "selectElementEvents.length;"));

  std::vector<std::string> select_element_events = {
      content::EvalJs(GetWebContents(), "selectElementEvents[0];")
          .ExtractString(),
      content::EvalJs(GetWebContents(), "selectElementEvents[1];")
          .ExtractString(),
  };

  EXPECT_THAT(select_element_events, testing::ElementsAre("input", "change"));
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
          var selectmenufocus = false;
          var selectmenuinput = false;
          var selectmenuchange = false;
          var selectmenublur = false;
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
           <selectmenu id="country"
           onfocus="selectmenufocus = true" oninput="selectmenuinput = true"
           onchange="selectmenuchange = true" onblur="selectmenublur = true" >
           <option value="" selected="yes">--</option>
           <option value="CA">Canada</option>
           <option value="US">United States</option>
           </selectmenu><br>
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

  // Checks that all the events were fired for the selectmenu field.
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectmenufocus;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectmenuinput;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectmenuchange;"));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(), "selectmenublur;"));
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
  SetTestProfile(browser()->profile(), profile);

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
  SetTestProfile(browser()->profile(), profile);

  GURL url =
      embedded_test_server()->GetURL("/autofill/read_only_field_test.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  EXPECT_EQ(addr_line1, GetFieldValueById("address"));
  EXPECT_EQ(company_name, GetFieldValueById("company"));
}

// TODO(https://crbug.com/1279102): Check back if flakiness is fixed now.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       NoAutofillSugggestionForCompanyName) {
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
  SetTestProfile(browser()->profile(), profile);

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
  EXPECT_EQ("15125551234", GetFieldValueById("PHONE_HOME_WHOLE_NUMBER"));
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
  SetTestProfile(browser()->profile(), profile);

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
  std::vector<AutofillProfile> profiles;
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
    profiles.push_back(profile);
  }
  SetTestProfiles(browser()->profile(), &profiles);

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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
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

  ::metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // If only some form fields are tagged with autocomplete types, then the
  // number of input elements will not match the number of fields when autofill
  // triees to preview or fill.
  histogram_tester().ExpectUniqueSample("Autofill.NumElementsMatchesNumFields",
                                        true, 2);

  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
}

// Test that we do not fill formless non-checkout forms when we enable the
// formless form restrictions. This test differes from the NoAutocomplete
// version of the the test in that at least one of the fields has an
// autocomplete attribute, so autofill will always be aware of the existence
// of the form.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestBase, SomeAutocomplete) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "/autofill/formless_some_autocomplete.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  ::metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // If only some form fields are tagged with autocomplete types, then the
  // number of input elements will not match the number of fields when autofill
  // triees to preview or fill.
  histogram_tester().ExpectUniqueSample("Autofill.NumElementsMatchesNumFields",
                                        true, 2);

  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
}

// Test that we do not fill formless non-checkout forms when we enable the
// formless form restrictions.
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestBase, AllAutocomplete) {
  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "/autofill/formless_all_autocomplete.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(AutofillFlow(GetElementById("firstname"), this));

  ::metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // If all form fields are tagged with autocomplete types, we make them all
  // available to be filled.
  histogram_tester().ExpectUniqueSample("Autofill.NumElementsMatchesNumFields",
                                        true, 2);

  EXPECT_EQ("Milton", GetFieldValueById("firstname"));
  EXPECT_EQ("4120 Freidrich Lane", GetFieldValueById("address"));
  EXPECT_EQ("TX", GetFieldValueById("state"));
  EXPECT_EQ("Austin", GetFieldValueById("city"));
  EXPECT_EQ("Initech", GetFieldValueById("company"));
  EXPECT_EQ("red.swingline@initech.com", GetFieldValueById("email"));
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
}

// Test that an 'onchange' event is not fired when a <selectmenu> preview
// suggestion is shown or hidden.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest,
                       NoEventFiredWhenExitingSelectMenuPreview) {
  // It is hard to test that an event will not happen in the future, but we
  // assume that applying similar operations on two elements in sequence results
  // in a consistent order of events triggered by the operations. So the test
  // strategy here is to first trigger a preview on `state` select, and then
  // select an element on `other`.

  CreateTestProfile();
  GURL url = embedded_test_server()->GetURL(
      "/autofill/form_selectmenu_preview_no_onchange.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Show autofill preview.
  ASSERT_TRUE(
      AutofillFlow(GetElementById("firstname"), this, {.do_accept = false}));

  // Hide autofill preview.
  content::RenderWidgetHost* render_widget_host =
      GetWebContents()->GetRenderWidgetHostView()->GetRenderWidgetHost();
  SendKeyToPopupAndWait(ui::DomKey::ESCAPE,
                        {ObservedUiEvents::kSuggestionsHidden},
                        render_widget_host);
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
      enabled.push_back({blink::features::kBrowsingTopicsXHR, {}});
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
  static_cast<BrowserAutofillManager*>(cross_driver->autofill_manager())
      ->SetTestDelegate(test_delegate());

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
  static_cast<BrowserAutofillManager*>(cross_driver->autofill_manager())
      ->SetTestDelegate(test_delegate());

  auto Wait = [this]() { DoNothingAndWait(base::Seconds(2)); };
  ASSERT_TRUE(AutofillFlow(GetElementById("CREDIT_CARD_NUMBER"), this,
                           {.after_focus = base::BindLambdaForTesting(Wait),
                            .execution_target = cross_frame_host}));
}

// TODO(https://crbug.com/1175735): Check back if flakiness is fixed now.
IN_PROC_BROWSER_TEST_P(AutofillInteractiveFencedFrameTest,
                       DeletingFrameUnderSuggestion) {
  CreateTestProfile();

  // Main frame is on a.com, fenced frame is on b.com.
  GURL url =
      https_server()->GetURL("a.com", "/autofill/cross_origin_iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
  static_cast<BrowserAutofillManager*>(cross_driver->autofill_manager())
      ->SetTestDelegate(test_delegate());

  // Focus the form in the iframe/fenced frame and simulate choosing a
  // suggestion via keyboard.
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

  // The popup should have disappeared with the iframe.
  EXPECT_FALSE(IsPopupShown());
}

INSTANTIATE_TEST_SUITE_P(AutofillInteractiveTest,
                         AutofillInteractiveFencedFrameTest,
                         ::testing::Values(FrameType::kFencedFrame,
                                           FrameType::kIFrame));

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
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone_form1"));
}

// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone_form1"));

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
  EXPECT_EQ("15125551234", GetFieldValueById("phone_form2"));
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
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
}

// Test that we can autofill forms that dynamically change the visibility of a
// field after it's autofilled.
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicFormFill_FirstElementDisappears) {
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
}

// Test that we can autofill forms that dynamically change the element that
// has been clicked on, even though the form has no name.
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicFormFill_FirstElementDisappearsNoNameForm) {
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
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
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicFormFill_FirstElementDisappearsBadnameUnowned) {
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
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
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
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicFormFill_FirstElementDisappearsUnowned) {
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
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
  ASSERT_TRUE(std::move(refill).Wait());

  EXPECT_EQ("Milton Waddams", GetFieldValueById("cc-name"));
  EXPECT_EQ("4111111111111111", GetFieldValueById("cc-num"));
  EXPECT_EQ("09", GetFieldValueById("cc-exp-month"));
  EXPECT_EQ("2999", GetFieldValueById("cc-exp-year"));
  EXPECT_EQ("", GetFieldValueById("cc-csc"));
}

void DoDynamicChangingFormFill_SelectUpdated(
    AutofillInteractiveTestDynamicForm* test,
    net::EmbeddedTestServer* test_server,
    bool should_test_selectmenu,
    bool should_test_async_update) {
  test->CreateTestProfile();
  GURL url = test_server->GetURL(
      "a.com",
      base::StringPrintf(
          ("/autofill/dynamic_form_select_or_selectmenu_options_change.html"
           "?is_selectmenu=%s&is_async=%s"),
          should_test_selectmenu ? "true" : "false",
          should_test_async_update ? "true" : "false"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(test->browser(), url));

  // Check that the test page correctly parsed the 'is_selectmenu' GET parameter
  // by checking type of the inserted field.
  auto has_n_controls_of_type = [](const std::string& control_type,
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
      should_test_selectmenu
          ? base::BindRepeating(has_n_controls_of_type, "selectmenu", 1)
          : base::BindRepeating(has_n_controls_of_type, "select-one", 2)));

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
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_SelectUpdated) {
  DoDynamicChangingFormFill_SelectUpdated(this, embedded_test_server(),
                                          /*should_test_selectmenu=*/false,
                                          /*should_test_async_update=*/false);
}

// Test that we can Autofill dynamically changing selectmenus that have options
// added and removed.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_SelectMenuUpdated) {
  DoDynamicChangingFormFill_SelectUpdated(this, embedded_test_server(),
                                          /*should_test_selectmenu=*/true,
                                          /*should_test_async_update=*/false);
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed, when the updating occurs asynchronously.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_SelectUpdatedAsync) {
  DoDynamicChangingFormFill_SelectUpdated(this, embedded_test_server(),
                                          /*should_test_selectmenu=*/false,
                                          /*should_test_async_update=*/true);
}

// Test that we can Autofill dynamically changing selectmenus that have options
// added and removed, when the updating occurs asynchronously.
IN_PROC_BROWSER_TEST_F(AutofillInteractiveTestDynamicForm,
                       DynamicChangingFormFill_SelectMenuUpdatedAsync) {
  DoDynamicChangingFormFill_SelectUpdated(this, embedded_test_server(),
                                          /*should_test_selectmenu=*/true,
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone_form1"));
}

// Test that we can Autofill dynamically changing selects that have options
// added and removed for forms with no names if the NameForAutofill of the first
// field matches.
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
}

// Test that we can Autofill dynamically generated synthetic forms if the
// NameForAutofill of the first field matches.
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone_syntheticform1"));
}

// Test that we can Autofill dynamically synthetic forms when the select options
// change if the NameForAutofill of the first field matches
// TODO(https://crbug.com/1297560): Check back if flakiness is fixed now.
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
  EXPECT_EQ("15125551234", GetFieldValueById("phone"));
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

  // Short hand for ExpectbucketCount:
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
    // TODO(accessibility): fix console error/warnings and insantiate
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

    EnterValues();
  }

  void SetUpServer() {
    SetTestUrlResponse(R"(
        <html><body>
        <form id='form' method='POST' action='/success.html'>
        Name: <input type='text' id='name'><br>
        Address: <input type='text' id='address'><br>
        City: <input type='text' id='city'><br>
        ZIP: <input type='text' id='zip'><br>
        State: <select id='state'>
          <option value='CA'>CA</option>
          <option value='WA'>WA</option>
        </select><br>
        </form>
    )");
    SetResponseForUrlPath("/success.html", "<html><body>Happy times!");
    SetResponseForUrlPath("/xhr", "<foo>Happy times!</foo>");
  }

  void EnterValues() {
    TestAutofillManagerWaiter waiter(
        *autofill_manager(), {AutofillManagerEvent::kTextFieldDidChange});
    // Normally we would enter the state last, but we don't have a
    // kSelectElementDidChange event, yet. Therefore, we just wait until
    // the second text field was reported to the autofill manager.
    ASSERT_TRUE(
        EnterTextsIntoFields({{GetElementById("name"), "Sarah"},
                              {GetElementById("state"), "WA"},
                              {GetElementById("address"), "123 Main Road"}},
                             this, GetWebContents()));
    ASSERT_TRUE(waiter.Wait(2u));
  }

  std::map<std::u16string, std::u16string> GetExpectedValues() {
    return std::map<std::u16string, std::u16string>{
        {u"name", u"Sarah"},
        {u"address", u"123 Main Road"},
        {u"city", u""},
        {u"zip", u""},
        {u"state", u"WA"}};
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
  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(*autofill_manager(),
              OnFormSubmittedImpl(SubmittedValuesAre(GetExpectedValues()),
                                  /*known_success=*/false,
                                  mojom::SubmissionSource::FORM_SUBMISSION))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  ExecuteScript("document.getElementById('form').submit();");
  run_loop.Run();
}

// Tests that non-link-click, renderer-inititiated navigation triggers a
// submission event in BrowserAutofillManager.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       ProbableSubmission) {
  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(SubmittedValuesAre(GetExpectedValues()),
                          /*known_success=*/false,
                          mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  // Add a delay before navigating away to avoid race conditions. This is
  // appropriate since we're faking user interaction here.
  ExecuteScript(
      "setTimeout(() => { window.location.assign('/success.html'); }, 50);");
  run_loop.Run();
}

// Tests that a same document navigation can trigger a form submission.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       SameDocumentNavigation) {
  base::RunLoop run_loop;
  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(SubmittedValuesAre(GetExpectedValues()),
                          /*known_success=*/true,
                          mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Simulate form submission.
  ExecuteScript(
      R"(
      // Same document navigation:
      document.getElementById('form').style.display = 'none';
      const url = new URL(window.location);
      url.searchParams.set('foo', 'bar');
      window.history.pushState({}, '', url);

      // Hide form, which is the trigger for the submission event.
      document.getElementById('form').style.display = 'none';
      )");
  run_loop.Run();
}

// Tests that an XHR request can indicate a form submission.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       XhrSuccededAndHideForm) {
  base::RunLoop run_loop;

  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(*autofill_manager(),
              OnFormSubmittedImpl(SubmittedValuesAre(GetExpectedValues()),
                                  /*known_success=*/true,
                                  mojom::SubmissionSource::XHR_SUCCEEDED))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Simulate form submission.
  ExecuteScript(
      R"(
      // SubmissionSource::XHR_SUCCEEDED is triggered if an XHR is observed
      // after the form has been made invisible.
      document.getElementById('form').style.display = 'none';

      const xhr = new XMLHttpRequest();
      xhr.open('GET', '/xhr', true);
      xhr.send(null);
      )");
  run_loop.Run();
}

// Tests that an XHR request can indicate a form submission - even if the form
// is deleted from the DOM.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillInteractiveFormSubmissionTest,
                       XhrSuccededAndDeleteForm) {
  base::RunLoop run_loop;

  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(*autofill_manager(),
              OnFormSubmittedImpl(SubmittedValuesAre(GetExpectedValues()),
                                  /*known_success=*/true,
                                  mojom::SubmissionSource::XHR_SUCCEEDED))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  // Simulate form submission.
  ExecuteScript(
      R"(
      // SubmissionSource::XHR_SUCCEEDED is triggered if an XHR is observed
      // after the form has been deleted.
      const form = document.getElementById('form');
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
  base::RunLoop run_loop;

  // Ensure that only expected form submissions are recorded.
  EXPECT_CALL(*autofill_manager(), OnFormSubmittedImpl).Times(0);
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(SubmittedValuesAre(GetExpectedValues()),
                          /*known_success=*/true,
                          mojom::SubmissionSource::DOM_MUTATION_AFTER_XHR))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

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
            document.getElementById('form').style.display = 'none';
          }, 50);
      }
      xhr.send(null);
      )");
  run_loop.Run();
}

}  // namespace autofill
