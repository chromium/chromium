// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/cpp/input_method_test_interface_constants.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace crosapi {
namespace {

using ::crosapi::mojom::InputMethodTestInterface;
using ::crosapi::mojom::InputMethodTestInterfaceAsyncWaiter;

bool IsInputMethodTestInterfaceAvailable() {
  return chromeos::LacrosService::Get()
             ->IsAvailable<crosapi::mojom::TestController>() &&
         chromeos::LacrosService::Get()
                 ->GetInterfaceVersion<crosapi::mojom::TestController>() >=
             static_cast<int>(
                 crosapi::mojom::TestController::MethodMinVersions::
                     kBindInputMethodTestInterfaceMinVersion);
}

int GetInputMethodTestInterfaceVersion() {
  return chromeos::LacrosService::Get()
      ->GetInterfaceVersion<crosapi::mojom::InputMethodTestInterface>();
}

// Used to parameterize these tests.
struct TestParam {
  // Enables kExoExtendedConfirmComposition, which uses an extended Wayland API
  // for ConfirmCompositionText.
  bool extended_confirm_composition = false;

  // Enables fixes for b/268467697.
  // Enables the following Lacros feature flags:
  // - WaylandKeepSelectionFix
  // - WaylandCancelComposition
  //
  // Will not be true if `extended_confirm_composition` is false.
  bool fix_268467697 = false;

  // Enables fixes for b/265853952.
  // Enables the following Ash feature flags:
  // - AlwaysConfirmComposition
  //
  // Will not be true if `extended_confirm_composition` is false.
  bool fix_265853952 = false;
};

// Binds an InputMethodTestInterface to Ash-Chrome, which allows these tests to
// execute IME operations from Ash-Chrome.
// `required_versions` are the `MethodMinVersion` values of all the test methods
// from InputMethodTestInterface that will be used by the test.
// `required_test_capabilities` is a list of all test-only capabilities that Ash
// needs to support. Returns an unbound remote if the current version of
// InputMethodTestInterface does not support the required test methods or
// capabilities.
mojo::Remote<InputMethodTestInterface> BindInputMethodTestInterface(
    const TestParam& test_param,
    std::initializer_list<InputMethodTestInterface::MethodMinVersions>
        required_versions,
    const std::vector<base::StringPiece>& required_test_capabilities = {}) {
  // TODO(b/238838841): Remove the `required_versions` check once all tested
  // versions of Ash in skew tests support `HasCapabilities`.
  if (!IsInputMethodTestInterfaceAvailable() ||
      GetInputMethodTestInterfaceVersion() <
          static_cast<int>(std::max(required_versions))) {
    return {};
  }

  // Bind an `InputMethodTestInterface`.
  mojo::Remote<InputMethodTestInterface> remote;
  crosapi::mojom::TestControllerAsyncWaiter test_controller_async_waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
  test_controller_async_waiter.BindInputMethodTestInterface(
      remote.BindNewPipeAndPassReceiver());

  if (required_test_capabilities.empty()) {
    return remote;
  }

  // Check if all the required test capabilities are satisfied.
  if (GetInputMethodTestInterfaceVersion() <
      static_cast<int>(InputMethodTestInterface::MethodMinVersions::
                           kHasCapabilitiesMinVersion)) {
    return {};
  }
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(remote.get());
  bool has_capabilities;
  input_method_async_waiter.HasCapabilities(
      std::vector<std::string>(required_test_capabilities.begin(),
                               required_test_capabilities.end()),
      &has_capabilities);
  if (!has_capabilities) {
    return {};
  }
  return remote;
}

// |browser| is a browser instance that will render `html`.
bool RenderHtmlInLacros(Browser* browser, const std::string& html) {
  const GURL url(base::StrCat({"data:text/html,", html}));
  if (!ui_test_utils::NavigateToURL(browser, url)) {
    return false;
  }

  std::string window_id = lacros_window_utility::GetRootWindowUniqueId(
      BrowserView::GetBrowserViewForBrowser(browser)
          ->frame()
          ->GetNativeWindow()
          ->GetRootWindow());
  EXPECT_TRUE(browser_test_util::WaitForWindowCreation(window_id));
  return true;
}

content::WebContents* GetActiveWebContents(Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

// Renders a focused input field in `browser`.
// Returns the ID of the input field.
std::string RenderAutofocusedInputFieldInLacros(Browser* browser) {
  if (!RenderHtmlInLacros(
          browser, R"(<input type="text" id="test-input" autofocus/>)")) {
    return "";
  }
  return "test-input";
}

// Renders a focused contenteditable div in `browser`.
// Returns the ID of the div.
std::string RenderAutofocusedContentEditableInLacros(Browser* browser) {
  if (!RenderHtmlInLacros(
          browser,
          R"(<div id="contenteditable-test-input" contenteditable></div>)")) {
    return "";
  }
  // <div>s do not have an autofocus attribute, so focus it manually.
  std::ignore = ExecJs(GetActiveWebContents(browser), R"(
    document.getElementById('contenteditable-test-input').focus();
  )");
  return "contenteditable-test-input";
}

struct Modifiers {
  bool alt;
  bool control;
  bool meta;
  bool shift;

  ui::EventFlags ToFlags() const {
    ui::EventFlags flags = ui::EF_NONE;
    if (control) {
      flags |= ui::EF_CONTROL_DOWN;
    }
    if (shift) {
      flags |= ui::EF_SHIFT_DOWN;
    }
    if (meta) {
      flags |= ui::EF_COMMAND_DOWN;
    }
    if (alt) {
      flags |= ui::EF_ALT_DOWN;
    }
    return flags;
  }
};

auto IsKeyboardEvent(const base::StringPiece type,
                     const base::StringPiece key,
                     const base::StringPiece code,
                     int key_code,
                     Modifiers modifiers = {}) {
  return base::test::IsJson(content::JsReplace(
      R"({
        "type": $1,
        "key": $2,
        "code": $3,
        "keyCode": $4,
        "altKey": $5,
        "ctrlKey": $6,
        "metaKey": $7,
        "shiftKey": $8,
      })",
      type, key, code, key_code, modifiers.alt, modifiers.control,
      modifiers.meta, modifiers.shift));
}

auto IsKeyDownEvent(const base::StringPiece key,
                    const base::StringPiece code,
                    int key_code,
                    Modifiers modifiers = {}) {
  return IsKeyboardEvent("keydown", key, code, key_code, modifiers);
}

auto IsKeyUpEvent(const base::StringPiece key,
                  const base::StringPiece code,
                  int key_code,
                  Modifiers modifiers = {}) {
  return IsKeyboardEvent("keyup", key, code, key_code, modifiers);
}

auto IsKeyPressEvent(const base::StringPiece key,
                     const base::StringPiece code,
                     int key_code,
                     Modifiers modifiers = {}) {
  return IsKeyboardEvent("keypress", key, code, key_code, modifiers);
}

auto IsCompositionEvent(const base::StringPiece type,
                        const base::StringPiece data) {
  return base::test::IsJson(content::JsReplace(
      R"({
        "type": $1,
        "data": $2,
      })",
      type, data));
}

auto IsCompositionStartEvent() {
  return IsCompositionEvent("compositionstart", "");
}

auto IsCompositionUpdateEvent(const base::StringPiece data) {
  return IsCompositionEvent("compositionupdate", data);
}

auto IsCompositionEndEvent() {
  return IsCompositionEvent("compositionend", "");
}

enum class CompositionState { kComposing, kNotComposing };

auto IsInputEvent(const base::StringPiece type,
                  const base::StringPiece input_type,
                  const absl::optional<base::StringPiece> data,
                  CompositionState composition_state) {
  const bool is_composing = composition_state == CompositionState::kComposing;

  if (!data.has_value()) {
    return base::test::IsJson(content::JsReplace(
        R"({
        "type": $1,
        "inputType": $2,
        "data": null,
        "isComposing": $3,
      })",
        type, input_type, is_composing));
  }

  return base::test::IsJson(content::JsReplace(
      R"({
        "type": $1,
        "inputType": $2,
        "data": $3,
        "isComposing": $4,
      })",
      type, input_type, *data, is_composing));
}

auto IsBeforeInputEvent(const base::StringPiece input_type,
                        const absl::optional<base::StringPiece> data,
                        CompositionState composition_state) {
  return IsInputEvent("beforeinput", input_type, data, composition_state);
}

auto IsInputEvent(const base::StringPiece input_type,
                  const absl::optional<base::StringPiece> data,
                  CompositionState composition_state) {
  return IsInputEvent("input", input_type, data, composition_state);
}

class InputEventListener {
 public:
  explicit InputEventListener(content::WebContents* web_contents)
      : messages_(web_contents) {}

  base::Value WaitForMessage() {
    std::string event_message;
    if (!messages_.WaitForMessage(&event_message)) {
      return {};
    }

    return base::test::ParseJson(event_message);
  }

  bool HasMessages() { return messages_.HasMessages(); }

 private:
  content::DOMMessageQueue messages_;
};

// Listens for web input events from `element_id`.
InputEventListener ListenForInputEvents(content::WebContents* web_content,
                                        base::StringPiece element_id) {
  const std::string script = content::JsReplace(
      R"(elem = document.getElementById($1);
         function extractEventData(e) {
           if (e instanceof CompositionEvent) {
             return {type: e.type, data: e.data};
           }
           if (e instanceof InputEvent) {
             return {
               type: e.type,
               isComposing: e.isComposing,
               inputType: e.inputType,
               data: e.data
             };
           }
           if (e instanceof KeyboardEvent) {
             return {
               type: e.type,
               key: e.key,
               code: e.code,
               keyCode: e.keyCode,
               altKey: e.altKey,
               ctrlKey: e.ctrlKey,
               metaKey: e.metaKey,
               shiftKey: e.shiftKey
             };
           }
           return {};
         }
         function recordEvent(e) {
           window.domAutomationController.send(extractEventData(e));
         }
         elem.addEventListener('keydown', recordEvent);
         elem.addEventListener('keypress', recordEvent);
         elem.addEventListener('keyup', recordEvent);
         elem.addEventListener('beforeinput', recordEvent);
         elem.addEventListener('input', recordEvent);
         elem.addEventListener('compositionstart', recordEvent);
         elem.addEventListener('compositionupdate', recordEvent);
         elem.addEventListener('compositionend', recordEvent);)",
      element_id);
  std::ignore = ExecJs(web_content, script);
  return InputEventListener(web_content);
}

// Waits for the contents of an input field with ID `element_id` to become
// `expected_text`, with the selection as `expected_selection`.
// For checking the text, this uses the DOM property `value`.
// For checking the selection, this uses the DOM properties
// `selectionStart` and `selectionEnd`.
// Returns true if the conditions are met within 3 seconds.
// Returns false otherwise.
bool WaitUntilInputFieldHasText(content::WebContents* web_content,
                                base::StringPiece element_id,
                                base::StringPiece expected_text,
                                const gfx::Range& expected_selection) {
  const std::string script = content::JsReplace(
      R"(new Promise((resolve) => {
        let retriesLeft = 10;
        elem = document.getElementById($1);
        function checkValue() {
          // Handle <input> elements.
          if (elem.value == $2 &&
              elem.selectionStart == $3 &&
              elem.selectionEnd == $4) {
            return resolve(true);
          }
          // Handle contenteditable elements.
          const selection = window.getSelection();
          if (elem.contains(selection.anchorNode) &&
              elem.contains(selection.focusNode) &&
              selection.anchorNode.textContent == $2 &&
              selection.anchorOffset == $3 &&
              selection.focusOffset == $4) {
            return resolve(true);
          }
          if (retriesLeft == 0) return resolve(false);
          retriesLeft--;
          setTimeout(checkValue, 300);
        }
        checkValue();
      }))",
      element_id, expected_text, static_cast<int>(expected_selection.start()),
      static_cast<int>(expected_selection.end()));
  return EvalJs(web_content, script).ExtractBool();
}

// Sets the contents of the input field with ID `element_id` to be `text`, with
// the text selection at `selection`.
bool SetInputFieldText(content::WebContents* web_content,
                       base::StringPiece element_id,
                       base::StringPiece text,
                       const gfx::Range& selection) {
  const std::string script = content::JsReplace(
      R"(elem = document.getElementById($1);
        elem.value = $2;
        elem.selectionStart = $3;
        elem.selectionEnd = $4;)",
      element_id, text, static_cast<int>(selection.start()),
      static_cast<int>(selection.end()));
  return ExecJs(web_content, script);
}

mojom::KeyEventPtr CreateKeyPressEvent(
    ui::DomKey dom_key,
    ui::DomCode dom_code,
    ui::KeyboardCode key_code = ui::KeyboardCode::VKEY_UNKNOWN,
    ui::EventFlags flags = ui::EF_NONE) {
  return mojom::KeyEvent::New(
      mojom::KeyEventType::kKeyPress, static_cast<int>(dom_key),
      static_cast<int>(dom_code), static_cast<int>(key_code), flags);
}

mojom::KeyEventPtr CreateKeyReleaseEvent(
    ui::DomKey dom_key,
    ui::DomCode dom_code,
    ui::KeyboardCode key_code = ui::KeyboardCode::VKEY_UNKNOWN,
    ui::EventFlags flags = ui::EF_NONE) {
  return mojom::KeyEvent::New(
      mojom::KeyEventType::kKeyRelease, static_cast<int>(dom_key),
      static_cast<int>(dom_code), static_cast<int>(key_code), flags);
}

class KeySequenceBuilder {
 public:
  KeySequenceBuilder Press(
      ui::DomKey dom_key,
      ui::DomCode dom_code,
      ui::KeyboardCode key_code = ui::KeyboardCode::VKEY_UNKNOWN) && {
    UpdateModifiersFromDomKey(dom_key, true);
    key_events_.push_back(CreateKeyPressEvent(dom_key, dom_code, key_code,
                                              active_modifiers_.ToFlags()));
    return std::move(*this);
  }

  KeySequenceBuilder Release(
      ui::DomKey dom_key,
      ui::DomCode dom_code,
      ui::KeyboardCode key_code = ui::KeyboardCode::VKEY_UNKNOWN) && {
    UpdateModifiersFromDomKey(dom_key, false);
    key_events_.push_back(CreateKeyReleaseEvent(dom_key, dom_code, key_code,
                                                active_modifiers_.ToFlags()));
    return std::move(*this);
  }

  KeySequenceBuilder PressAndRelease(
      ui::DomKey dom_key,
      ui::DomCode dom_code,
      ui::KeyboardCode key_code = ui::KeyboardCode::VKEY_UNKNOWN) && {
    return std::move(*this)
        .Press(dom_key, dom_code, key_code)
        .Release(dom_key, dom_code, key_code);
  }

  std::vector<mojom::KeyEventPtr> Build() && { return std::move(key_events_); }

 private:
  void UpdateModifiersFromDomKey(ui::DomKey dom_key, bool pressed) {
    if (dom_key == ui::DomKey::ALT) {
      active_modifiers_.alt = pressed;
    }
    if (dom_key == ui::DomKey::CONTROL) {
      active_modifiers_.control = pressed;
    }
    if (dom_key == ui::DomKey::META) {
      active_modifiers_.meta = pressed;
    }
    if (dom_key == ui::DomKey::SHIFT) {
      active_modifiers_.shift = pressed;
    }
  }

  std::vector<mojom::KeyEventPtr> key_events_;
  Modifiers active_modifiers_;
};

// Sends the key events to the input method. The input method will not handle
// the given key events.
void SendKeyEventsSync(
    InputMethodTestInterfaceAsyncWaiter& input_method_async_waiter,
    std::vector<mojom::KeyEventPtr> key_events) {
  uint64_t key_event_id;
  for (mojom::KeyEventPtr& key_event : key_events) {
    input_method_async_waiter.SendKeyEvent(std::move(key_event), &key_event_id);
    input_method_async_waiter.KeyEventHandled(key_event_id, false);
  }
}

// Convenient version of `SendKeyEventsSync` for a single key event.
void SendKeyEventSync(
    InputMethodTestInterfaceAsyncWaiter& input_method_async_waiter,
    mojom::KeyEventPtr key_event) {
  std::vector<mojom::KeyEventPtr> key_events;
  key_events.push_back(std::move(key_event));
  SendKeyEventsSync(input_method_async_waiter, std::move(key_events));
}

// Sends the key event to the input method. The input method will handle the
// given key event by running `callback`.
void SendKeyEventAsync(
    InputMethodTestInterfaceAsyncWaiter& input_method_async_waiter,
    mojom::KeyEventPtr key_event,
    base::OnceCallback<bool(InputMethodTestInterfaceAsyncWaiter&)> callback) {
  uint64_t key_event_id;
  input_method_async_waiter.SendKeyEvent(std::move(key_event), &key_event_id);
  const bool handled = std::move(callback).Run(input_method_async_waiter);
  input_method_async_waiter.KeyEventHandled(key_event_id, handled);
}

void WaitUntilSurroundingTextIs(
    InputMethodTestInterfaceAsyncWaiter& input_method_async_waiter,
    const std::string& expected_surrounding_text,
    const gfx::Range& expected_selection_range) {
  std::string surrounding_text;
  gfx::Range selection_range;
  while (true) {
    input_method_async_waiter.WaitForNextSurroundingTextChange(
        &surrounding_text, &selection_range);
    if (surrounding_text == expected_surrounding_text &&
        selection_range == expected_selection_range) {
      break;
    }
  }
}

// Keep this fixture simple: only use this to enable / disable feature flags.
class InputMethodLacrosBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<TestParam> {
 public:
  InputMethodLacrosBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_lacros_features;
    if (GetParam().fix_268467697) {
      enabled_lacros_features.push_back(features::kWaylandKeepSelectionFix);
      enabled_lacros_features.push_back(features::kWaylandCancelComposition);
    }
    feature_list_override_.InitWithFeatures(enabled_lacros_features,
                                            /*disabled_features=*/{});
  }

  void SetUp() override {
    std::vector<std::string> enabled_ash_features;
    if (GetParam().fix_265853952) {
      enabled_ash_features.push_back("AlwaysConfirmComposition");
    }
    if (GetParam().extended_confirm_composition) {
      enabled_ash_features.push_back("ExoExtendedConfirmComposition");
    }
    if (!enabled_ash_features.empty()) {
      StartUniqueAshChrome(
          enabled_ash_features, /*disabled_features=*/{},
          /*additional_cmdline_switches=*/{},
          "Use shared Ash once all the feature flags above are default.");
    }
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_override_;
};

INSTANTIATE_TEST_SUITE_P(
    InputMethodLacrosBrowserTestAllParams,
    InputMethodLacrosBrowserTest,
    ::testing::Values(
        // All features off.
        TestParam{},
        // Enable `extended_confirm_composition` first.
        TestParam{.extended_confirm_composition = true},
        // Combos of `fix_268467697` and `fix_265853952`.
        TestParam{.extended_confirm_composition = true, .fix_268467697 = true},
        TestParam{.extended_confirm_composition = true, .fix_265853952 = true},
        TestParam{.extended_confirm_composition = true,
                  .fix_268467697 = true,
                  .fix_265853952 = true}));

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       FocusingInputFieldSendsFocus) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(), {InputMethodTestInterface::MethodMinVersions::
                           kWaitForFocusMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());

  input_method_async_waiter.WaitForFocus();
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       CommitTextInsertsTextInInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::kCommitTextMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.CommitText("hello");

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello", gfx::Range(5)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       CommitTextUpdatesSurroundingText) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::kCommitTextMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.CommitText("abc");
  std::string surrounding_text;
  gfx::Range selection_range;
  input_method_async_waiter.WaitForNextSurroundingTextChange(&surrounding_text,
                                                             &selection_range);

  EXPECT_EQ(surrounding_text, "abc");
  EXPECT_EQ(selection_range, gfx::Range(3));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       CommitTextReplacesCompositionText) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::kCommitTextMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "hello ",
                                gfx::Range(6)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  input_method_async_waiter.SetComposition("world", 5);
  ASSERT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello world", gfx::Range(11)));

  input_method_async_waiter.CommitText("abc");

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello abc", gfx::Range(9)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       CommitEmptyTextDeletesCompositionText) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::kCommitTextMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  input_method_async_waiter.SetComposition("hello", 5);
  ASSERT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello", gfx::Range(5)));

  input_method_async_waiter.CommitText("");

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "", gfx::Range(0)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       CommitTextReplacesSelection) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::kCommitTextMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "hello",
                                gfx::Range(1, 3)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.CommitText("abc");

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "habclo", gfx::Range(4)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       CommitTextTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  InputEventListener event_listener =
      ListenForInputEvents(GetActiveWebContents(browser()), id);

  input_method_async_waiter.CommitText("hello");
  input_method_async_waiter.CommitText(" world");

  EXPECT_THAT(event_listener.WaitForMessage(),
              IsBeforeInputEvent("insertText", "hello",
                                 CompositionState::kNotComposing));
  EXPECT_THAT(
      event_listener.WaitForMessage(),
      IsInputEvent("insertText", "hello", CompositionState::kNotComposing));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsBeforeInputEvent("insertText", " world",
                                 CompositionState::kNotComposing));
  EXPECT_THAT(
      event_listener.WaitForMessage(),
      IsInputEvent("insertText", " world", CompositionState::kNotComposing));
  EXPECT_FALSE(event_listener.HasMessages());
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       CommitTextWhileHandlingKeyEventTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  InputEventListener event_listener =
      ListenForInputEvents(GetActiveWebContents(browser()), id);

  SendKeyEventAsync(
      input_method_async_waiter,
      CreateKeyPressEvent(ui::DomKey::FromCharacter('.'), ui::DomCode::PERIOD),
      base::BindOnce(
          [](InputMethodTestInterfaceAsyncWaiter& input_method_async_waiter) {
            input_method_async_waiter.CommitText("。");
            return true;
          }));
  SendKeyEventSync(input_method_async_waiter,
                   CreateKeyReleaseEvent(ui::DomKey::FromCharacter('.'),
                                         ui::DomCode::PERIOD));

  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyDownEvent(".", "Period", 190));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyPressEvent("。", "Period", 12290));
  EXPECT_THAT(
      event_listener.WaitForMessage(),
      IsBeforeInputEvent("insertText", "。", CompositionState::kNotComposing));
  EXPECT_THAT(
      event_listener.WaitForMessage(),
      IsInputEvent("insertText", "。", CompositionState::kNotComposing));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyUpEvent(".", "Period", 190));
  EXPECT_FALSE(event_listener.HasMessages());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "。", gfx::Range(1)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SetCompositionInsertsCompositionInEmptyInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.SetComposition("hello", 3);

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello", gfx::Range(3)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SetCompositionInsertsCompositionAtStartOfInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, " world",
                                gfx::Range(0)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.SetComposition("hello", 5);

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello world", gfx::Range(5)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SetCompositionInsertsCompositionAtEndOfInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "hello ",
                                gfx::Range(6)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.SetComposition("world", 5);

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello world", gfx::Range(11)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SetCompositionInsertsCompositionInMiddleOfInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "held",
                                gfx::Range(2)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.SetComposition("llo wor", 3);

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello world", gfx::Range(5)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SetCompositionReplacesCompositionInInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  input_method_async_waiter.SetComposition("hello", 4);

  input_method_async_waiter.SetComposition("abc", 2);

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc", gfx::Range(2)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SetCompositionTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  InputEventListener event_listener =
      ListenForInputEvents(GetActiveWebContents(browser()), id);

  input_method_async_waiter.SetComposition("hello", 4);
  input_method_async_waiter.SetComposition("", 0);

  EXPECT_THAT(event_listener.WaitForMessage(), IsCompositionStartEvent());
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsBeforeInputEvent("insertCompositionText", "hello",
                                 CompositionState::kComposing));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsCompositionUpdateEvent("hello"));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsInputEvent("insertCompositionText", "hello",
                           CompositionState::kComposing));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsBeforeInputEvent("insertCompositionText", "",
                                 CompositionState::kComposing));
  EXPECT_THAT(event_listener.WaitForMessage(), IsCompositionUpdateEvent(""));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsInputEvent("insertCompositionText", absl::nullopt,
                           CompositionState::kComposing));
  EXPECT_THAT(event_listener.WaitForMessage(), IsCompositionEndEvent());
  EXPECT_FALSE(event_listener.HasMessages());
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SetCompositionUpdatesSurroundingText) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::kCommitTextMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.SetComposition("abc", 3);
  std::string surrounding_text;
  gfx::Range selection_range;
  input_method_async_waiter.WaitForNextSurroundingTextChange(&surrounding_text,
                                                             &selection_range);

  EXPECT_EQ(surrounding_text, "abc");
  EXPECT_EQ(selection_range, gfx::Range(3));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SendKeyEventNotHandledTypesInEmptyTextField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A,
                           ui::KeyboardCode::VKEY_A)
          .PressAndRelease(ui::DomKey::FromCharacter('b'), ui::DomCode::US_B,
                           ui::KeyboardCode::VKEY_B)
          .PressAndRelease(ui::DomKey::FromCharacter('c'), ui::DomCode::US_C,
                           ui::KeyboardCode::VKEY_C)
          .Build());

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc", gfx::Range(3)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SendBackspaceDeletesNonEmptyTextField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "hello",
                                gfx::Range(3)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::BACKSPACE, ui::DomCode::BACKSPACE)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "helo", gfx::Range(2)));

  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::BACKSPACE, ui::DomCode::BACKSPACE)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hlo", gfx::Range(1)));

  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::BACKSPACE, ui::DomCode::BACKSPACE)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "lo", gfx::Range(0)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SendLeftArrowKeyWithSelectionCollapsesSelectionLeft) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "abcde",
                                gfx::Range(1, 4)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  WaitUntilSurroundingTextIs(input_method_async_waiter, "abcde",
                             gfx::Range(1, 4));

  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::ARROW_LEFT, ui::DomCode::ARROW_LEFT)
          .Build());

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abcde", gfx::Range(1)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SendRightArrowKeyWithSelectionCollapsesSelectionRight) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "abcde",
                                gfx::Range(1, 4)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  WaitUntilSurroundingTextIs(input_method_async_waiter, "abcde",
                             gfx::Range(1, 4));

  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::ARROW_RIGHT, ui::DomCode::ARROW_RIGHT)
          .Build());

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abcde", gfx::Range(4)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SendKeyEventShortcutsModifiesSelection) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion},
          {kInputMethodTestCapabilitySendKeyModifiers});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id,
                                "abc abc abc", gfx::Range(0)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  // Press Ctrl-A with: Control down, A down, A up, Control up
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .Press(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .PressAndRelease(ui::DomKey::FromCharacter('a'), ui::DomCode::US_A)
          .Release(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc abc abc", gfx::Range(0, 11)));

  // Press Ctrl+Left.
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .Press(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .PressAndRelease(ui::DomKey::ARROW_LEFT, ui::DomCode::ARROW_LEFT)
          .Release(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc abc abc", gfx::Range(8)));

  // Press Ctrl+Right with a different order.
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .Press(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .Press(ui::DomKey::ARROW_RIGHT, ui::DomCode::ARROW_RIGHT)
          .Release(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .Release(ui::DomKey::ARROW_RIGHT, ui::DomCode::ARROW_RIGHT)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc abc abc", gfx::Range(11)));

  // Press Shift+Left.
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .Press(ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT)
          .PressAndRelease(ui::DomKey::ARROW_LEFT, ui::DomCode::ARROW_LEFT)
          .Release(ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc abc abc", gfx::Range(10, 11)));

  // Press Ctrl+Shift+Left.
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .Press(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .Press(ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT)
          .PressAndRelease(ui::DomKey::ARROW_LEFT, ui::DomCode::ARROW_LEFT)
          .Release(ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT)
          .Release(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc abc abc", gfx::Range(8, 11)));

  // Press Ctrl+Shift+Right with a different order.
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .Press(ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT)
          .Press(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .Press(ui::DomKey::ARROW_RIGHT, ui::DomCode::ARROW_RIGHT)
          .Release(ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT)
          .Release(ui::DomKey::ARROW_RIGHT, ui::DomCode::ARROW_RIGHT)
          .Release(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
          .Build());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc abc abc", gfx::Range(11, 11)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SetCompositionWhileHandlingKeyEventTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  InputEventListener event_listener =
      ListenForInputEvents(GetActiveWebContents(browser()), id);

  SendKeyEventAsync(
      input_method_async_waiter,
      CreateKeyPressEvent(ui::DomKey::FromCharacter('g'), ui::DomCode::US_G),
      base::BindOnce(
          [](InputMethodTestInterfaceAsyncWaiter& input_method_async_waiter) {
            input_method_async_waiter.SetComposition("ㅎ", 1);
            return true;
          }));
  SendKeyEventSync(
      input_method_async_waiter,
      CreateKeyReleaseEvent(ui::DomKey::FromCharacter('g'), ui::DomCode::US_G));

  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyDownEvent("Process", "KeyG", 229));
  EXPECT_THAT(event_listener.WaitForMessage(), IsCompositionStartEvent());
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsBeforeInputEvent("insertCompositionText", "ㅎ",
                                 CompositionState::kComposing));
  EXPECT_THAT(event_listener.WaitForMessage(), IsCompositionUpdateEvent("ㅎ"));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsInputEvent("insertCompositionText", "ㅎ",
                           CompositionState::kComposing));
  EXPECT_THAT(event_listener.WaitForMessage(), IsKeyUpEvent("g", "KeyG", 71));
  EXPECT_FALSE(event_listener.HasMessages());
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "ㅎ", gfx::Range(1)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SendKeyEventTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  InputEventListener event_listener =
      ListenForInputEvents(GetActiveWebContents(browser()), id);

  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder().PressAndRelease('a', ui::DomCode::US_A).Build());

  EXPECT_THAT(event_listener.WaitForMessage(), IsKeyDownEvent("a", "KeyA", 65));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyPressEvent("a", "KeyA", 97));
  EXPECT_THAT(
      event_listener.WaitForMessage(),
      IsBeforeInputEvent("insertText", "a", CompositionState::kNotComposing));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsInputEvent("insertText", "a", CompositionState::kNotComposing));
  EXPECT_THAT(event_listener.WaitForMessage(), IsKeyUpEvent("a", "KeyA", 65));
  EXPECT_FALSE(event_listener.HasMessages());
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       SendKeyEventModifiersTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion},
          {kInputMethodTestCapabilitySendKeyModifiers});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  InputEventListener event_listener =
      ListenForInputEvents(GetActiveWebContents(browser()), id);

  // Press Control+Alt+Shift+Meta and release them in a different order.
  SendKeyEventsSync(input_method_async_waiter,
                    KeySequenceBuilder()
                        .Press(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
                        .Press(ui::DomKey::ALT, ui::DomCode::ALT_RIGHT)
                        .Press(ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT)
                        .Press(ui::DomKey::META, ui::DomCode::META_LEFT)
                        .Release(ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT)
                        .Release(ui::DomKey::ALT, ui::DomCode::ALT_RIGHT)
                        .Release(ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT)
                        .Release(ui::DomKey::META, ui::DomCode::META_LEFT)
                        .Build());

  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyDownEvent("Control", "ControlLeft", 17, {.control = true}));
  EXPECT_THAT(
      event_listener.WaitForMessage(),
      IsKeyDownEvent("Alt", "AltRight", 18, {.alt = true, .control = true}));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyDownEvent("Shift", "ShiftLeft", 16,
                             {.alt = true, .control = true, .shift = true}));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyDownEvent(
                  "Meta", "MetaLeft", 91,
                  {.alt = true, .control = true, .meta = true, .shift = true}));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyUpEvent("Shift", "ShiftLeft", 16,
                           {.alt = true, .control = true, .meta = true}));
  EXPECT_THAT(
      event_listener.WaitForMessage(),
      IsKeyUpEvent("Alt", "AltRight", 18, {.control = true, .meta = true}));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyUpEvent("Control", "ControlLeft", 17, {.meta = true}));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyUpEvent("Meta", "MetaLeft", 91));
  EXPECT_FALSE(event_listener.HasMessages());
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       DeleteSurroundingTextAtEnd) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityDeleteSurroundingText});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "abcd",
                                gfx::Range(4)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  WaitUntilSurroundingTextIs(input_method_async_waiter, "abcd", gfx::Range(4));
  input_method_async_waiter.DeleteSurroundingText(1, 0);
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc", gfx::Range(3)));

  WaitUntilSurroundingTextIs(input_method_async_waiter, "abc", gfx::Range(3));
  input_method_async_waiter.DeleteSurroundingText(2, 0);
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "a", gfx::Range(1)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       DeleteSurroundingTextAtBeginning) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityDeleteSurroundingText});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "abcd",
                                gfx::Range(0)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  WaitUntilSurroundingTextIs(input_method_async_waiter, "abcd", gfx::Range(0));
  input_method_async_waiter.DeleteSurroundingText(0, 1);
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "bcd", gfx::Range(0)));

  WaitUntilSurroundingTextIs(input_method_async_waiter, "bcd", gfx::Range(0));
  input_method_async_waiter.DeleteSurroundingText(0, 2);
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "d", gfx::Range(0)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       DeleteSurroundingTextInMiddle) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityDeleteSurroundingText});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "abcd",
                                gfx::Range(2)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  WaitUntilSurroundingTextIs(input_method_async_waiter, "abcd", gfx::Range(2));
  input_method_async_waiter.DeleteSurroundingText(1, 1);
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "ad", gfx::Range(1)));

  WaitUntilSurroundingTextIs(input_method_async_waiter, "ad", gfx::Range(1));
  input_method_async_waiter.DeleteSurroundingText(1, 1);
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "", gfx::Range(0)));
}

IN_PROC_BROWSER_TEST_P(
    InputMethodLacrosBrowserTest,
    DeleteSurroundingTextInvalidStillDeletesWithLengthCappedAtStart) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityDeleteSurroundingText});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "abcd",
                                gfx::Range(2)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  WaitUntilSurroundingTextIs(input_method_async_waiter, "abcd", gfx::Range(2));
  input_method_async_waiter.DeleteSurroundingText(3, 1);

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "d", gfx::Range(0)));
}

IN_PROC_BROWSER_TEST_P(
    InputMethodLacrosBrowserTest,
    DeleteSurroundingTextInvalidStillDeletesWithLengthCappedAtEnd) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityDeleteSurroundingText});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "abcd",
                                gfx::Range(2)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  WaitUntilSurroundingTextIs(input_method_async_waiter, "abcd", gfx::Range(2));
  input_method_async_waiter.DeleteSurroundingText(1, 3);

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "a", gfx::Range(1)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       ConfirmCompositionWithNoSelectionAndNoComposition) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityConfirmComposition,
           kInputMethodTestCapabilityExtendedConfirmComposition});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "hello",
                                gfx::Range(3)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  WaitUntilSurroundingTextIs(input_method_async_waiter, "hello", gfx::Range(3));

  input_method_async_waiter.ConfirmComposition();

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello", gfx::Range(3)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       ConfirmCompositionWithNoSelectionAndComposition) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityConfirmComposition,
           kInputMethodTestCapabilityExtendedConfirmComposition});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  input_method_async_waiter.SetComposition("hello", 3);
  WaitUntilSurroundingTextIs(input_method_async_waiter, "hello", gfx::Range(3));

  input_method_async_waiter.ConfirmComposition();

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello", gfx::Range(3)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       ConfirmCompositionWithSelectionAndNoComposition) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityConfirmComposition,
           kInputMethodTestCapabilityExtendedConfirmComposition});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "hello",
                                gfx::Range(1, 3)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  WaitUntilSurroundingTextIs(input_method_async_waiter, "hello",
                             gfx::Range(1, 3));

  input_method_async_waiter.ConfirmComposition();

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hello", gfx::Range(1, 3)));
}

// See b/265853952.
IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       ConfirmCompositionWithIncorrectSurroundingText) {
  if (!GetParam().fix_265853952) {
    return;
  }

  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kSetCompositionMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kWaitForNextSurroundingTextChangeMinVersion},
          {kInputMethodTestCapabilityConfirmComposition,
           kInputMethodTestCapabilityAlwaysConfirmComposition,
           kInputMethodTestCapabilityExtendedConfirmComposition});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedContentEditableInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  // Simulate an input field that gives unreliable surrounding text by appending
  // '!' to the surrounding text on every 'input' event. This approach changes
  // the surrounding text without canceling any composition or affecting the
  // contents of the input field.
  std::ignore = ExecJs(
      GetActiveWebContents(browser()),
      content::JsReplace(
          R"(document.getElementById($1).addEventListener('input', (e) => {
         e.target.insertAdjacentHTML('beforeend', '<span>!</span>');
       });)",
          id));

  input_method_async_waiter.SetComposition("a", 1);
  WaitUntilSurroundingTextIs(input_method_async_waiter, "a!", gfx::Range(1));

  // This ConfirmComposition should still go through even if the surrounding
  // text information is incorrect for this input field. If it doesn't, then
  // the next CommitText will replace the current composition of 'a'.
  input_method_async_waiter.ConfirmComposition();
  input_method_async_waiter.CommitText("b");

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "ab", gfx::Range(2)));
}

// See b/267944900 for more information.
IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       EscapeAfterResetKeepsSelection) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "a",
                                gfx::Range(1)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  // Trigger a selection change via JavaScript to reset the IME but retain the
  // surrounding text and selection.
  std::ignore = ExecJs(
      GetActiveWebContents(browser()),
      content::JsReplace(R"(document.getElementById($1).select();)", id));
  WaitUntilSurroundingTextIs(input_method_async_waiter, "a", gfx::Range(0, 1));

  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::ESCAPE, ui::DomCode::ESCAPE)
          .Build());

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "a", gfx::Range(0, 1)));
}

// See crbug.com/1434957 for more information.
IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       DeleteSurroundingTextAfterResetDeletes) {
  if (!GetParam().fix_265853952) {
    return;
  }

  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion},
          {kInputMethodTestCapabilityDeleteSurroundingText});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  ASSERT_TRUE(SetInputFieldText(GetActiveWebContents(browser()), id, "abcd",
                                gfx::Range(3, 4)));
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  WaitUntilSurroundingTextIs(input_method_async_waiter, "abcd",
                             gfx::Range(3, 4));
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::BACKSPACE, ui::DomCode::BACKSPACE,
                           ui::KeyboardCode::VKEY_BACK)
          .Build());

  WaitUntilSurroundingTextIs(input_method_async_waiter, "abc",
                             gfx::Range(3, 3));
  input_method_async_waiter.DeleteSurroundingText(1, 0);
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "ab", gfx::Range(2)));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest, DeadKeyTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion},
          {kInputMethodTestCapabilityChangeInputMethod});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();
  InputEventListener event_listener =
      ListenForInputEvents(GetActiveWebContents(browser()), id);

  // Switch to an US International input method, which has dead keys.
  // TODO: crbug.com/1344058 - This currently depends on the Linux machine
  // running the test to have "us(intl)" in the correct XKB layout directory.
  // Refactor Ozone to use PathService so that this directory can be controlled
  // to make this test hermetic.
  input_method_async_waiter.InstallAndSwitchToInputMethod(
      crosapi::mojom::InputMethod::New(/*xkb_layout=*/"us(intl)"));
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::DeadKeyFromCombiningCharacter(U'\u0301'),
                           ui::DomCode::QUOTE, ui::KeyboardCode::VKEY_OEM_7)
          .Build());

  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyDownEvent("Dead", "Quote", 222));
  EXPECT_THAT(event_listener.WaitForMessage(),
              IsKeyUpEvent("Dead", "Quote", 222));
}

IN_PROC_BROWSER_TEST_P(InputMethodLacrosBrowserTest,
                       ChangingInputMethodUpdatesKeyLayout) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          GetParam(),
          {InputMethodTestInterface::MethodMinVersions::kWaitForFocusMinVersion,
           InputMethodTestInterface::MethodMinVersions::
               kKeyEventHandledMinVersion},
          {kInputMethodTestCapabilityChangeInputMethod});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  // Switch to an AZERTY input method (French).
  // TODO(crbug.com/1344058): This currently depends on the Linux machine
  // running the test to have "fr" in the correct XKB layout directory. Refactor
  // Ozone to use PathService so that this directory can be controlled to make
  // this test hermetic.
  input_method_async_waiter.InstallAndSwitchToInputMethod(
      crosapi::mojom::InputMethod::New(/*xkb_layout=*/"fr"));
  SendKeyEventsSync(
      input_method_async_waiter,
      KeySequenceBuilder()
          .PressAndRelease(ui::DomKey::FromCharacter('q'), ui::DomCode::US_Q,
                           ui::KeyboardCode::VKEY_Q)
          .PressAndRelease(ui::DomKey::FromCharacter('w'), ui::DomCode::US_W,
                           ui::KeyboardCode::VKEY_W)
          .Build());

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "az", gfx::Range(2)));
}

}  // namespace
}  // namespace crosapi
