// Copyright 2022 The Chromium Authors. All rights reserved.
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

bool IsInputMethodTestInterfaceAvailable() {
  return chromeos::LacrosService::Get()
             ->IsAvailable<crosapi::mojom::TestController>() &&
         chromeos::LacrosService::Get()->GetInterfaceVersion(
             crosapi::mojom::TestController::Uuid_) >=
             static_cast<int>(
                 crosapi::mojom::TestController::MethodMinVersions::
                     kBindInputMethodTestInterfaceMinVersion);
}

// Binds an InputMethodTestInterface to Ash-Chrome, which allows these tests to
// execute IME operations from Ash-Chrome.
mojo::Remote<crosapi::mojom::InputMethodTestInterface>
BindInputMethodTestInterface() {
  mojo::Remote<crosapi::mojom::InputMethodTestInterface> remote;
  if (!IsInputMethodTestInterfaceAvailable()) {
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

  browser_test_util::WaitForWindowCreation(
      lacros_window_utility::GetRootWindowUniqueId(
          BrowserView::GetBrowserViewForBrowser(browser)
              ->frame()
              ->GetNativeWindow()
              ->GetRootWindow()));
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
// `expected_text`. Returns true if the contents become `expected_text`
// within 3 seconds. Returns false otherwise.
bool WaitUntilInputFieldHasText(content::WebContents* web_content,
                                base::StringPiece element_id,
                                base::StringPiece expected_text) {
  const std::string script = content::JsReplace(
      R"(new Promise((resolve) => {
        let retriesLeft = 10;
        elem = document.getElementById($1);
        function checkValue() {
          if (elem.value == $2) return resolve(true);
          if (retriesLeft == 0) return resolve(false);
          retriesLeft--;
          setTimeout(checkValue, 300);
        }
        checkValue();
      }))",
      element_id, expected_text);
  return ExecJs(web_content, script);
}

using InputMethodLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       FocusingInputFieldSendsFocus) {
  if (!IsInputMethodTestInterfaceAvailable())
    GTEST_SKIP() << "Unsupported ash version";
  RenderAutofocusedInputFieldInLacros(browser());
  mojo::Remote<crosapi::mojom::InputMethodTestInterface> input_method =
      BindInputMethodTestInterface();
  crosapi::mojom::InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());

  input_method_async_waiter.WaitForFocus();
}

IN_PROC_BROWSER_TEST_F(InputMethodLacrosBrowserTest,
                       CommitTextInsertsTextInInputField) {
  if (!IsInputMethodTestInterfaceAvailable())
    GTEST_SKIP() << "Unsupported ash version";
  const std::string id = RenderAutofocusedInputFieldInLacros(browser());
  mojo::Remote<crosapi::mojom::InputMethodTestInterface> input_method =
      BindInputMethodTestInterface();
  crosapi::mojom::InputMethodTestInterfaceAsyncWaiter input_method_async_waiter(
      input_method.get());
  input_method_async_waiter.WaitForFocus();

  input_method_async_waiter.CommitText("hello");

  EXPECT_TRUE(
      WaitUntilInputFieldHasText(GetActiveWebContents(browser()), id, "hello"));
}

}  // namespace
