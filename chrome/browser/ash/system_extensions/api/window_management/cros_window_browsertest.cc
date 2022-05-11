// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/aura/window.h"

namespace {

static constexpr char kEventListenerCode[] = R"(
  self.addEventListener('message', async (event) => {
    try {
      await cros_test();
      event.source.postMessage("PASS");
    } catch (e) {
      console.log(e.message);
      event.source.postMessage("FAIL - Check console LOGS");
    }
  });
)";

static constexpr char kPostTestStart[] = R"(
  async function startTest() {
    const saw_message = new Promise(resolve => {
      navigator.serviceWorker.onmessage = event => {
        resolve(event.data);
      };
    });
    const registration = await navigator.serviceWorker.ready;
    registration.active.postMessage('test');
    return await saw_message;
  }

  if (document.readyState !== "complete") {
    window.onload = startTest();
  } else {
    startTest();
  }
)";

class CrosWindowBrowserTest : public InProcessBrowserTest {
 public:
  CrosWindowBrowserTest() {
    feature_list_.InitAndEnableFeature(ash::features::kSystemExtensions);

    installation_ =
        web_app::TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
  ~CrosWindowBrowserTest() override = default;

  // TODO(b/210737979):
  // Remove switch toggles when service workers are supported on
  // chrome-untrusted://
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kSystemExtensionsDebug);
    command_line->AppendSwitchASCII(
        switches::kEnableBlinkFeatures,
        "BlinkExtensionChromeOS,BlinkExtensionChromeOSWindowManagement");
  }

  void RunTest(base::StringPiece test_code) {
    // Initialize embedded test server.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Serve dependencies for the service worker (i.e. asserts).
    embedded_test_server()->ServeFilesFromSourceDirectory(
        base::FilePath("third_party/blink/web_tests/resources"));

    // Register test code with listener and dependencies as .js file.
    const std::string js_code =
        base::StrCat({"self.importScripts('/testharness.js',"
                      "'/testharness-helpers.js',"
                      "'/system_extensions/cros_window_test_utils.js');",
                      test_code, kEventListenerCode});
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [js_code](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != "/test_service_worker.js") {
            return nullptr;
          }
          std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
              std::make_unique<net::test_server::BasicHttpResponse>());
          http_response->set_code(net::HTTP_OK);
          http_response->set_content(js_code);
          http_response->set_content_type("text/javascript");
          return std::move(http_response);
        }));

    // Register test code js as service worker.
    embedded_test_server()->StartAcceptingConnections();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     "register('/test_service_worker.js');"));

    // Post message to service worker listener to trigger test and evaluate.
    EXPECT_EQ("PASS",
              EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     kPostTestStart));
  }

 protected:
  std::unique_ptr<web_app::TestSystemWebAppInstallation> installation_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowSetOrigin) {
  const char test_code[] = R"(
async function cros_test() {
  let [window] = await chromeos.windowManagement.getWindows();

  let x = window.screenLeft;
  let y = window.screenTop;
  x += 10;
  y += 10;

  await setOriginAndTest(x, y);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowSetBounds) {
  const char test_code[] = R"(
async function cros_test() {
  let [window] = await chromeos.windowManagement.getWindows();

  let x = window.screenLeft;
  let y = window.screenTop;
  let width = window.width;
  let height = window.height;
  x += 10;
  y += 10;
  width -= 100;
  height -= 100;

  await setBoundsAndTest(x, y, width, height);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowSetFullscreen) {
  const char test_code[] = R"(
async function cros_test() {
  // Check that the window begins in a non-fullscreen state.
  await assertWindowState("normal");

  // Check that window can be fullscreened and repeating maintains fullscreen.
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(true);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, FullscreenMinMax) {
  const char test_code[] = R"(
async function cros_test() {
  // Check that window begins in non-fullscreen state.
  await assertWindowState("normal");

  // Minimized->Fullscreen->Maximized->Minimized
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await maximizeAndTest();
  await minimizeAndTest();

  // Reversing above: Minimized<-Fullscreen<-Maximized<-Minimized
  await maximizeAndTest();
  await setFullscreenAndTest(true);
  await minimizeAndTest();
}
  )";

  RunTest(test_code);
}

// When unsetting fullscreen from a previously normal or maximized window,
// the window state should return to its previous state.
IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, UnsetFullscreenNonMinimized) {
  const char test_code[] = R"(
async function cros_test() {
  await assertWindowState("normal");

  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("normal");

  await maximizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("maximized");
}
  )";

  RunTest(test_code);
}

// When unsetting fullscreen from a previously minimized window,
// the window state should return to the last non-minimized state.
IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, UnsetFullscreenMinimized) {
  const char test_code[] = R"(
async function cros_test() {
  await assertWindowState("normal");

  // Normal->Minimized->Fullscreen should unfullscreen to normal.
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("normal");

  // Normal->Fullscreen->Minimized->Fullscreen should unfullscreen to normal.
  await setFullscreenAndTest(true);
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("normal");

  // Maximized->Minimized->Fullscreen should unfullscreen to normal.
  await maximizeAndTest();
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("maximized");

  // Maximized->Fullscreen->Minimized->Fullscreen should unfullscreen to normal.
  await setFullscreenAndTest(true);
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("maximized");
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowMaximize) {
  const char test_code[] = R"(
async function cros_test() {
  await assertWindowState("normal");

  await maximizeAndTest();

  // Repeating maximize should not change any properties.
  await maximizeAndTest();
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowMinimize) {
  const char test_code[] = R"(
async function cros_test() {
  await assertWindowState("normal");

  await minimizeAndTest();

  // Repeating minimize should not change any properties.
  await minimizeAndTest();
}
  )";

  RunTest(test_code);
}

// Checks that focusing a non-visible unfocused window correctly sets focus.
IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowFocusSingle) {
  const char test_code[] = R"(
async function cros_test() {
  await assertWindowState("normal");
  {
    let [window] = await chromeos.windowManagement.getWindows();
    assert_true(window.isFocused);
  }

  await minimizeAndTest();
  {
    let [window] = await chromeos.windowManagement.getWindows();
    assert_false(window.isFocused);
  }

  await focusAndTest();
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowFocusMulti) {
  // Open browser instance to take focus.
  chrome::NewWindow(browser());

  const char test_code[] = R"(
async function cros_test() {
  // async window retriever with stable window ordering after first retrieval.
  let getWindows;

  {
    let [first_window, second_window] =
        await chromeos.windowManagement.getWindows();
    getWindows = async function() {
      let [first_returned_window, second_returned_window] =
          await chromeos.windowManagement.getWindows();
      assert_equals(first_window.id, first_returned_window.id);
      assert_equals(second_window.id, second_returned_window.id);
      return [first_returned_window, second_returned_window];
    };
  }

  {
    let [first_window, second_window] = await getWindows();
    // When focusing 1st window, it should have sole focus.
    first_window.focus();
  }

  {
    let [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // When focusing 2nd window, it should have sole focus.
    second_window.focus();

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Fullscreening a window does not focus an unfocused window.
    first_window.setFullscreen(true);

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // We can focus a fullscreen window.
    first_window.focus();

    [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // We can focus another window on top of a fullscreen window.
    second_window.focus();

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Minimizing focused window should pass focus to next window.
    second_window.minimize();

    [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Minimizing remaining window should lose focus.
    first_window.minimize();

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_false(second_window.isFocused);
  }
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowClose) {
  // Open browser instance to close outside of service worker.
  chrome::NewWindow(browser());

  aura::Window* initial = browser()->window()->GetNativeWindow();
  aura::Window* new_window =
      BrowserList::GetInstance()->GetLastActive()->window()->GetNativeWindow();

  ASSERT_NE(initial, new_window);

  // Set target id to crosWindow id of newly opened window as per instance
  // registry.
  std::string target_id;

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->InstanceRegistry().ForEachInstance(
      [&target_id, &new_window](const apps::InstanceUpdate& update) {
        if (update.Window()->GetToplevelWindow() == new_window) {
          CHECK(target_id.empty());
          target_id = update.InstanceId().ToString();
        }
      });

  std::string test_code = base::StringPrintf(R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.getWindows();
  assert_equals(windows.length, 2);

  let window_to_close = windows.find(window => window.id === "%1$s");
  assert_not_equals(undefined, window_to_close,
      `Could not find window with id: (%1$s);`);
  window_to_close.close();

  // TODO(b/221123297): Currently test will flake on close under stress.
  // Defer testing until on close event implemented
  // windows = await chromeos.windowManagement.getWindows();
  // assert_equals(windows.length, 1);
}
  )",
                                             target_id.c_str());

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowSWACrashTest) {
  // Finish installation of Sample SWA.
  installation_->WaitForAppInstall();

  // Wait for Sample SWA window to open.
  content::TestNavigationObserver navigation_observer(
      installation_->GetAppUrl());
  navigation_observer.StartWatchingNewWebContents();

  web_app::LaunchSystemWebAppAsync(browser()->profile(),
                                   installation_->GetType());

  navigation_observer.Wait();

  // Initial window contains service worker. Track new window as test subject.
  aura::Window* initial = browser()->window()->GetNativeWindow();
  aura::Window* new_window =
      BrowserList::GetInstance()->GetLastActive()->window()->GetNativeWindow();

  ASSERT_NE(initial, new_window);

  // Set target id to crosWindow id of newly opened window as per instance
  // registry.
  std::string target_id;

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->InstanceRegistry().ForEachInstance(
      [&target_id, &new_window](const apps::InstanceUpdate& update) {
        if (update.Window()->GetToplevelWindow() == new_window) {
          CHECK(target_id.empty());
          target_id = update.InstanceId().ToString();
        }
      });

  std::string test_code = base::StringPrintf(R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.getWindows();
  assert_equals(windows.length, 2);

  let swa_window = windows.find(window => window.id === "%1$s");
  assert_not_equals(undefined, swa_window,
      `Could not find window with id: (%1$s);`);

  swa_window.minimize();
  swa_window.focus();
  swa_window.maximize();
  swa_window.setFullscreen(true);
  swa_window.close();
}
  )",
                                             target_id.c_str());

  RunTest(test_code);
}

// Tests that the CrosWindowManagement object is an EventTarget.
IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowManagementEventTarget) {
  const char test_code[] = R"(
async function cros_test() {
  assert_true(chromeos.windowManagement instanceof EventTarget);

  return new Promise(resolve => {
    chromeos.windowManagement.addEventListener('testevent', e => {
      assert_equals(e.target, chromeos.windowManagement);
      resolve();
    });
    chromeos.windowManagement.dispatchEvent(new Event('testevent'));
  });
}
  )";

  RunTest(test_code);
}
