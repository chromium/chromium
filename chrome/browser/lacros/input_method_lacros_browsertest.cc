// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace crosapi {
namespace {

using ::crosapi::mojom::InputMethodTestInterface;
using ::crosapi::mojom::InputMethodTestInterfaceAsyncWaiter;

bool IsInputMethodTestInterfaceAvailable() {
  return chromeos::LacrosService::Get()
             ->IsAvailable<crosapi::mojom::TestController>() &&
         chromeos::LacrosService::Get()->GetInterfaceVersion(
             crosapi::mojom::TestController::Uuid_) >=
             static_cast<int>(
                 crosapi::mojom::TestController::MethodMinVersions::
                     kBindInputMethodTestInterfaceMinVersion);
}

int GetInputMethodTestInterfaceVersion() {
  return chromeos::LacrosService::Get()->GetInterfaceVersion(
      crosapi::mojom::InputMethodTestInterface::Uuid_);
}

// Binds an InputMethodTestInterface to Ash-Chrome, which allows these tests to
// execute IME operations from Ash-Chrome.
// `required_versions` are the `MethodMinVersion` values of all the test methods
// from InputMethodTestInterface that will be used by the test.
// Returns an unbound remote if the current version of InputMethodTestInterface
// does not support the required test methods.
mojo::Remote<InputMethodTestInterface> BindInputMethodTestInterface(
    std::initializer_list<InputMethodTestInterface::MethodMinVersions>
        required_versions) {
  mojo::Remote<InputMethodTestInterface> remote;
  if (!IsInputMethodTestInterfaceAvailable() ||
      GetInputMethodTestInterfaceVersion() <
          static_cast<int>(std::max(required_versions))) {
    return remote;
  }

  crosapi::mojom::TestControllerAsyncWaiter test_controller_async_waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
  test_controller_async_waiter.BindInputMethodTestInterface(
      remote.BindNewPipeAndPassReceiver());
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

// Renders a focused input field in `browser`.
// Returns the ID of the input field.
std::string RenderAutofocusedInputFieldInLacros(Browser* browser) {
  if (!RenderHtmlInLacros(
          browser, R"(<input type="text" id="test-input" autofocus/>)")) {
    return "";
  }
  return "test-input";
}

content::WebContents* GetActiveWebContents(Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

auto IsKeyboardEvent(const base::StringPiece type,
                     const base::StringPiece key,
                     const base::StringPiece code,
                     int key_code) {
  return base::test::IsJson(content::JsReplace(
      R"({
        "type": $1,
        "key": $2,
        "code": $3,
        "keyCode": $4,
      })",
      type, key, code, key_code));
}

auto IsKeyDownEvent(const base::StringPiece key,
                    const base::StringPiece code,
                    int key_code) {
  return IsKeyboardEvent("keydown", key, code, key_code);
}

auto IsKeyUpEvent(const base::StringPiece key,
                  const base::StringPiece code,
                  int key_code) {
  return IsKeyboardEvent("keyup", key, code, key_code);
}

auto IsKeyPressEvent(const base::StringPiece key,
                     const base::StringPiece code,
                     int key_code) {
  return IsKeyboardEvent("keypress", key, code, key_code);
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
               keyCode: e.keyCode
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
          if (elem.value == $2 &&
              elem.selectionStart == $3 &&
              elem.selectionEnd == $4) {
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

mojom::KeyEventPtr CreateKeyPressEvent(ui::DomKey dom_key,
                                       ui::DomCode dom_code) {
  return mojom::KeyEvent::New(mojom::KeyEventType::kKeyPress,
                              static_cast<int>(dom_key),
                              static_cast<int>(dom_code),
                              static_cast<int>(ui::KeyboardCode::VKEY_UNKNOWN));
}

mojom::KeyEventPtr CreateKeyReleaseEvent(ui::DomKey dom_key,
                                         ui::DomCode dom_code) {
  return mojom::KeyEvent::New(mojom::KeyEventType::kKeyRelease,
                              static_cast<int>(dom_key),
                              static_cast<int>(dom_code),
                              static_cast<int>(ui::KeyboardCode::VKEY_UNKNOWN));
}

std::vector<mojom::KeyEventPtr> CreateKeyPressAndReleaseEvents(
    ui::DomKey dom_key,
    ui::DomCode dom_code) {
  std::vector<mojom::KeyEventPtr> key_events;
  key_events.push_back(CreateKeyPressEvent(dom_key, dom_code));
  key_events.push_back(CreateKeyReleaseEvent(dom_key, dom_code));
  return key_events;
}

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

using InputMethodLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       FocusingInputFieldSendsFocus) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
          {InputMethodTestInterface::MethodMinVersions::
               kWaitForFocusMinVersion});
  if (!input_method.is_bound()) {
    GTEST_SKIP() << "Unsupported ash version";
  }
  RenderAutofocusedInputFieldInLacros(browser());
  InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());

  input_method_async_waiter.WaitForFocus();
}

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       CommitTextInsertsTextInInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       CommitTextUpdatesSurroundingText) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       CommitTextReplacesCompositionText) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       CommitEmptyTextDeletesCompositionText) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       CommitTextReplacesSelection) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       CommitTextTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       CommitTextWhileHandlingKeyEventTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SetCompositionInsertsCompositionInEmptyInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SetCompositionInsertsCompositionAtStartOfInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SetCompositionInsertsCompositionAtEndOfInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SetCompositionInsertsCompositionInMiddleOfInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SetCompositionReplacesCompositionInInputField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SetCompositionTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SetCompositionUpdatesSurroundingText) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SendKeyEventNotHandledTypesInEmptyTextField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

  SendKeyEventsSync(input_method_async_waiter,
                    CreateKeyPressAndReleaseEvents(
                        ui::DomKey::FromCharacter('a'), ui::DomCode::US_A));
  SendKeyEventsSync(input_method_async_waiter,
                    CreateKeyPressAndReleaseEvents(
                        ui::DomKey::FromCharacter('b'), ui::DomCode::US_B));
  SendKeyEventsSync(input_method_async_waiter,
                    CreateKeyPressAndReleaseEvents(
                        ui::DomKey::FromCharacter('c'), ui::DomCode::US_C));

  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "abc", gfx::Range(3)));
}

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SendBackspaceDeletesNonEmptyTextField) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

  SendKeyEventsSync(input_method_async_waiter,
                    CreateKeyPressAndReleaseEvents(ui::DomKey::BACKSPACE,
                                                   ui::DomCode::BACKSPACE));
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "helo", gfx::Range(2)));

  SendKeyEventsSync(input_method_async_waiter,
                    CreateKeyPressAndReleaseEvents(ui::DomKey::BACKSPACE,
                                                   ui::DomCode::BACKSPACE));
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "hlo", gfx::Range(1)));

  SendKeyEventsSync(input_method_async_waiter,
                    CreateKeyPressAndReleaseEvents(ui::DomKey::BACKSPACE,
                                                   ui::DomCode::BACKSPACE));
  EXPECT_TRUE(WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id,
                                         "lo", gfx::Range(0)));
}

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SetCompositionWhileHandlingKeyEventTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       SendKeyEventTriggersWebEvents) {
  mojo::Remote<InputMethodTestInterface> input_method =
      BindInputMethodTestInterface(
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

  SendKeyEventSync(input_method_async_waiter,
                   CreateKeyPressEvent('a', ui::DomCode::US_A));
  SendKeyEventSync(input_method_async_waiter,
                   CreateKeyReleaseEvent('a', ui::DomCode::US_A));

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

}  // namespace
}  // namespace crosapi
