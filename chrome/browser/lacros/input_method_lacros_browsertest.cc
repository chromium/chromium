// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
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
                       SetCompositionInsertsCompositionInInputField) {
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

}  // namespace
