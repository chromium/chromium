// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_flow_test_util.h"

#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_test_utils.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"

using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

namespace autofill {

namespace {

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

struct ShowAutofillSuggestionsParams {
  ShowMethod show_method = ShowMethod::ByArrow();
  int num_profile_suggestions = 1;
  size_t max_tries = 5;
  base::TimeDelta timeout = kAutofillFlowDefaultTimeout;
  std::optional<content::ToRenderFrameHost> execution_target = {};
};

// A helper function for showing the popup in AutofillFlow().
// Consider using AutofillFlow() instead.
[[nodiscard]] AssertionResult ShowAutofillSuggestions(
    const ElementExpr& e,
    AutofillUiTest* test,
    ShowAutofillSuggestionsParams p) {
  constexpr auto kSuggest = ObservedUiEvents::kSuggestionsShown;
  constexpr auto kPreview = ObservedUiEvents::kPreviewFormData;

  content::ToRenderFrameHost execution_target =
      p.execution_target.value_or(test->GetWebContents());
  content::RenderFrameHost* rfh = execution_target.render_frame_host();
  content::RenderWidgetHostView* view = rfh->GetView();

  auto ArrowDown = [&](std::list<ObservedUiEvents> exp) {
    return test->SendKeyToPageAndWait(ui::DomKey::ARROW_DOWN, std::move(exp),
                                      p.timeout);
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
    test->test_delegate()->SetExpectations(std::move(exp), p.timeout);
    content::SimulateMouseClickAt(test->GetWebContents(), 0,
                                  blink::WebMouseEvent::Button::kLeft, point);
    return test->test_delegate()->Wait();
  };

  // It seems that due to race conditions with Blink's layouting
  // (crbug.com/1175735#c9), the below focus events are sometimes too early:
  // Autofill closes the popup right away because it is outside of the content
  // area. To work around this, we attempt to bring up the Autofill popup
  // multiple times, with some delay.
  testing::Message m;
  m << __func__ << "(): with " << p.num_profile_suggestions
    << " profile suggestions.";
  bool field_was_focused_initially = IsFocusedField(e, rfh);
  for (size_t i = 1; i <= p.max_tries; ++i) {
    m << "\nIteration " << i << "/" << p.max_tries << ". ";
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
        m << "Trying to re-focus the field. ";
        if (AssertionResult b = BlurFocusedField(rfh); !b) {
          m << b.message();
        }
        if (AssertionResult b = FocusField(e, rfh); !b) {
          m << b.message();
        }
      }
    }

    bool has_preview = 0 < p.num_profile_suggestions;
    if (p.show_method.arrow) {
      // Press arrow down to open the popup and select first suggestion.
      // Depending on the platform, this requires one or two arrow-downs.
      if (!IsFocusedField(e, rfh)) {
        return AssertionFailure()
               << m << "Field " << *e << " must be focused. ";
      }
      if (AssertionResult b = has_preview ? ArrowDown({kPreview, kSuggest})
                                          : ArrowDown({kSuggest});
          !b) {
        m << "Cannot trigger and select first suggestion by arrow: "
          << b.message();
        continue;
      }
    } else if (p.show_method.character) {
      // Enter character to open the popup, but do not select an option.
      // If necessary, delete past iterations character first.
      if (!IsFocusedField(e, rfh)) {
        return AssertionFailure()
               << m << "Field " << *e << " must be focused. ";
      }
      if (i > 1) {
        if (AssertionResult b = Backspace(); !b) {
          m << "Cannot undo past iteration's key: " << b.message();
        }
      }
      std::string code = std::string("Key") + p.show_method.character;
      if (AssertionResult b = Char(code, {kSuggest}); !b) {
        m << "Cannot trigger suggestions by key: " << b.message();
        continue;
      }
    } else if (p.show_method.click) {
      // Click item to open the popup, but do not select an option.
      if (AssertionResult b = Click({kSuggest}); !b) {
        m << "Cannot trigger and select first suggestion by click: "
          << b.message();
        continue;
      }
    }
    LOG(WARNING) << (m << "Succeeded.");
    return AssertionSuccess();
  }
  return AssertionFailure()
         << m << "Couldn't show Autofill suggestions on " << *e << ". ";
}

struct AutofillSuggestionParams {
  int num_profile_suggestions = 1;
  int current_index = 0;
  int target_index = 0;
  bool expect_previews = true;
  base::TimeDelta timeout = kAutofillFlowDefaultTimeout;
  std::optional<content::ToRenderFrameHost> execution_target = {};
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
    bool has_preview = i < p.num_profile_suggestions && p.expect_previews;
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

  // All attempts to accept Autofill suggestions using keyboard "ENTER"
  // keystrokes will be ignored for the first 500ms after the popup is first
  // shown. This overrides this threshold.
  if (base::WeakPtr<AutofillSuggestionController> controller =
          ChromeAutofillClient::FromWebContentsForTesting(
              test->GetWebContents())
              ->suggestion_controller_for_testing()) {
    test_api(static_cast<AutofillPopupControllerImpl&>(*controller))
        .DisableThreshold(true);
  }

  constexpr auto kSuggestionsHidden = ObservedUiEvents::kSuggestionsHidden;
  constexpr auto kFill = ObservedUiEvents::kFormDataFilled;

  auto Enter = [&](std::list<ObservedUiEvents> exp) {
    return test->SendKeyToPopupAndWait(ui::DomKey::ENTER, std::move(exp),
                                       widget);
  };

  bool has_fill = p.target_index < p.num_profile_suggestions;
  if (AssertionResult a = SelectAutofillSuggestion(e, test, p); !a) {
    return a;
  }
  if (!(has_fill ? Enter({kFill, kSuggestionsHidden})
                 : Enter({kSuggestionsHidden}))) {
    return AssertionFailure()
           << __func__ << "(): Couldn't accept to " << p.target_index
           << "th suggestion with" << (has_fill ? "" : "out") << " fill";
  }
  return AssertionSuccess();
}

}  // namespace

// A helper function for focusing a field in AutofillFlow().
// Consider using AutofillFlow() instead.
[[nodiscard]] AssertionResult FocusField(
    const ElementExpr& e,
    content::ToRenderFrameHost execution_target) {
  if (IsFocusedField(e, execution_target)) {
    AssertionResult r = BlurFocusedField(execution_target);
    if (!r) {
      return r;
    }
  }
  return TriggerAndWaitForEvent(e, "focus", execution_target);
}

[[nodiscard]] AssertionResult AutofillFlow(const ElementExpr& e,
                                           AutofillUiTest* test,
                                           AutofillFlowParams p) {
  content::ToRenderFrameHost execution_target =
      p.execution_target.value_or(test->GetWebContents());

  if (p.do_focus) {
    AssertionResult a = FocusField(e, execution_target);
    if (!a) {
      return a;
    }
    if (p.after_focus) {
      p.after_focus.Run();
    }
  }

  if (p.do_show) {
    AssertionResult a = ShowAutofillSuggestions(
        e, test,
        {.show_method = p.show_method,
         .num_profile_suggestions = p.num_profile_suggestions,
         .max_tries = p.max_show_tries,
         .timeout = p.timeout,
         .execution_target = execution_target});
    if (!a) {
      return a;
    }
    if (p.after_show) {
      p.after_show.Run();
    }
  }

  if (p.do_select) {
    AssertionResult a = SelectAutofillSuggestion(
        e, test,
        {.num_profile_suggestions = p.num_profile_suggestions,
         .current_index = p.show_method.selects_first_suggestion() ? 0 : -1,
         .target_index = p.target_index,
         .expect_previews = p.expect_previews,
         .timeout = p.timeout,
         .execution_target = execution_target});
    if (!a) {
      return a;
    }
    if (p.after_select) {
      p.after_select.Run();
    }
  }

  if (p.do_accept) {
    AssertionResult a = AcceptAutofillSuggestion(
        e, test,
        {.num_profile_suggestions = p.num_profile_suggestions,
         .current_index = p.target_index,
         .target_index = p.target_index,
         .expect_previews = p.expect_previews,
         .timeout = p.timeout,
         .execution_target = execution_target});
    if (!a) {
      return a;
    }
    if (p.after_accept) {
      p.after_accept.Run();
    }
  }

  return AssertionSuccess();
}

}  // namespace autofill
