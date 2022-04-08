// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

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
    const std::string js_code = base::StrCat(
        {"self.importScripts('/testharness.js', '/testharness-helpers.js');",
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowSetOrigin) {
  const char test_code[] = R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.windows();
  const newBounds = DOMRect.fromRect(windows[0].bounds);
  newBounds.x += 10;
  newBounds.y += 10;
  windows[0].setOrigin(newBounds.x, newBounds.y);
  windows = await chromeos.windowManagement.windows();
  const actualBounds = windows[0].bounds;
  assert_weak_equals(actualBounds, newBounds,
      `SetOrigin should set origin without changing bounds`);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowSetBounds) {
  const char test_code[] = R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.windows();
  const newBounds = DOMRect.fromRect(windows[0].bounds);
  newBounds.x += 10;
  newBounds.y += 10;
  newBounds.width -= 100;
  newBounds.height -= 100;
  windows[0].setBounds(newBounds.x, newBounds.y,
      newBounds.width, newBounds.height);
  windows = await chromeos.windowManagement.windows();
  const actualBounds = windows[0].bounds;
 assert_weak_equals(actualBounds, newBounds, `SetBounds failed to set bounds`);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowSetFullscreen) {
  const char test_code[] = R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.windows();
  assert_false(windows[0].isFullscreen, "Window started in fullscreen.");

  windows[0].setFullscreen(true);
  windows = await chromeos.windowManagement.windows();
  assert_true(windows[0].isFullscreen, "setFullscreen(true) failed");

  windows[0].setFullscreen(false);
  windows = await chromeos.windowManagement.windows();
  assert_false(windows[0].isFullscreen, "setFullscreen(false) failed");
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, RepeatSetFullscreen) {
  const char test_code[] = R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.windows();
  assert_false(windows[0].isFullscreen, "Window started in fullscreen.");

  windows[0].setFullscreen(false);
  windows = await chromeos.windowManagement.windows();
  assert_false(windows[0].isFullscreen,
      "setFullscreen(false) set window to fullscreen");

  windows[0].setFullscreen(true);
  windows = await chromeos.windowManagement.windows();
  assert_true(windows[0].isFullscreen, "setFullscreen(true) failed");

  windows[0].setFullscreen(true);
  windows = await chromeos.windowManagement.windows();
  assert_true(windows[0].isFullscreen, "setFullscreen(true) failed");
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, FullscreenFromMinimised) {
  const char test_code[] = R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.windows();
  assert_false(windows[0].isFullscreen, "Window started in fullscreen.");

  windows[0].minimize();
  windows = await chromeos.windowManagement.windows();
  assert_true(windows[0].isMinimised);
  assert_false(windows[0].isVisible);

  windows[0].setFullscreen(true);
  windows = await chromeos.windowManagement.windows();
  assert_true(windows[0].isFullscreen);
  assert_true(windows[0].isVisible);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowMinimize) {
  const char test_code[] = R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.windows();
  assert_false(windows[0].isMinimised);
  assert_true(windows[0].isVisible);

  windows[0].minimize();
  windows = await chromeos.windowManagement.windows();
  assert_true(windows[0].isMinimised);
  assert_false(windows[0].isVisible);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowClose) {
  // Open browser instance to close outside of service worker.
  chrome::NewWindow(browser());

  const char test_code[] = R"(
async function cros_test() {
  let windows = await chromeos.windowManagement.windows();
  assert_equals(windows.length, 2);

  const window_to_close_index =
      windows[0].title == "Chromium - create service worker" ? 1 : 0;
  windows[window_to_close_index].close();

// TODO(b/221123297): Currently test will flake on close under stress.
// Defer testing until on close event implemented
  // windows = await chromeos.windowManagement.windows();
  // assert_equals(windows.length, 1);
}
  )";

  RunTest(test_code);
}
