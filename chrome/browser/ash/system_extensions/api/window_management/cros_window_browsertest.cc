// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

namespace {

static constexpr char kEventListenerCode[] = R"(
  self.addEventListener('message', async (event) => {
    try {
      await test();
      event.source.postMessage("PASS");
    } catch (e) {
      console.log(e.message);
      event.source.postMessage("FAIL - Check console LOGS");
    }
  });
)";

static constexpr char kPostTestStart[] = R"(
  (async () => {
    const saw_message = new Promise(resolve => {
      navigator.serviceWorker.onmessage = event => {
        resolve(event.data);
      };
    });
    const registration = await navigator.serviceWorker.ready;
    registration.active.postMessage('test');
    return await saw_message;
  })();
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

    // Register test code with listener as .js file.
    const std::string js_code = base::StrCat({test_code, kEventListenerCode});
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

    // Register test code js as service worker
    embedded_test_server()->StartAcceptingConnections();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     "register('/test_service_worker.js');"));

    // Post message to service worker listener to trigger test code and
    // evaluate.
    EXPECT_EQ("PASS",
              EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                     kPostTestStart));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowSetFullscreen) {
  const char test_code[] = R"(
async function test() {
  let windows = await chromeos.windowManagement.windows();
  if (windows[0].isFullscreen) {
    throw new Error("Window started in fullscreen.");
  }

  windows[0].setFullscreen(true);
  windows = await chromeos.windowManagement.windows();
  if (!windows[0].isFullscreen) {
    throw new Error("cros_window.setFullscreen(true) failed");
  }

  windows[0].setFullscreen(false);
  windows = await chromeos.windowManagement.windows();
  if (windows[0].isFullscreen) {
    throw new Error("cros_window.setFullscreen(false) failed");
  }
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, RepeatSetFullscreen) {
  const char test_code[] = R"(
async function test() {
  let windows = await chromeos.windowManagement.windows();
  if (windows[0].isFullscreen) {
    throw new Error("Window started in fullscreen.");
  }

  windows[0].setFullscreen(false);
  windows = await chromeos.windowManagement.windows();
  if (windows[0].isFullscreen) {
    throw new Error("setFullscreen(false) set window fullscreen");
  }

  windows[0].setFullscreen(true);
  windows = await chromeos.windowManagement.windows();
  if (!windows[0].isFullscreen) {
    throw new Error("setFullscreen(true) failed");
  }

  windows[0].setFullscreen(true);
  windows = await chromeos.windowManagement.windows();
  if (!windows[0].isFullscreen) {
    throw new Error("setFullscreen(true) unset window fullscreen");
  }
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, FullscreenFromMinimised) {
  const char test_code[] = R"(
async function test() {
  let windows = await chromeos.windowManagement.windows();
  if (windows[0].isFullscreen) {
    throw new Error("Window started in fullscreen.");
  }

  windows[0].minimize();
  windows = await chromeos.windowManagement.windows();
  if (!windows[0].isMinimised || windows[0].isVisible) {
    throw new Error("minimize() did not minimize window");
  }

  windows[0].setFullscreen(true);
  windows = await chromeos.windowManagement.windows();
  if (!windows[0].isFullscreen || !windows[0].isVisible) {
    throw new Error("setFullscreen(true) did not make window visible and fs");
  }
}
  )";

  RunTest(test_code);
}
