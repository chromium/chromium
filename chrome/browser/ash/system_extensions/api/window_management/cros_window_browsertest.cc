// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

constexpr SystemExtensionId kTestSystemExtensionId = {1, 2, 3, 4};

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

// Temporary observer to understand flaky test.
// TODO(crbug.com/1328079): Remove once the root cause for the flaky test is
// found.
class DebugServiceWorkerContextObserver
    : public content::ServiceWorkerContextObserver {
 public:
  explicit DebugServiceWorkerContextObserver(Profile* profile) {
    auto* worker_context =
        profile->GetDefaultStoragePartition()->GetServiceWorkerContext();
    context_observation_.Observe(worker_context);
  }
  ~DebugServiceWorkerContextObserver() override = default;

  void OnRegistrationCompleted(const GURL& scope) override {
    LOG(ERROR) << "Service Worker registered: " << scope;
  }

  void OnVersionActivated(int64_t version_id, const GURL& scope) override {
    LOG(ERROR) << "Version activated:\n"
               << "  scope: " << scope << "\n"
               << "  version_id: " << version_id;
  }

  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    LOG(ERROR) << "Version started running:\n"
               << "  script_url: " << running_info.script_url << "\n"
               << "  scope: " << running_info.scope << "\n"
               << "  version_id: " << version_id;
  }

  void OnVersionStoppedRunning(int64_t version_id) override {
    LOG(ERROR) << "Version stopped running:\n"
               << "  version_id: " << version_id;
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    LOG(ERROR) << "Context destroyed";
    context_observation_.Reset();
  }

 private:
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      context_observation_{this};
};

// Used to wait for a message to get added to the Service Worker console.
// Returns the first message added to the console.
class ServiceWorkerConsoleObserver
    : public content::ServiceWorkerContextObserver {
 public:
  ServiceWorkerConsoleObserver(Profile* profile, const GURL& scope)
      : profile_(profile), scope_(scope) {
    auto* worker_context =
        profile->GetDefaultStoragePartition()->GetServiceWorkerContext();
    worker_context->AddObserver(this);
  }
  ~ServiceWorkerConsoleObserver() override = default;

  // Get the first message added to the console since the observer was
  // constructed. Will wait if there are no messages yet.
  const std::u16string& WaitAndGetNextConsoleMessage() {
    LOG(ERROR) << "Wainting for console message.";
    if (!message_.has_value())
      run_loop_.Run();

    LOG(ERROR) << "Console message received.";
    return message_.value();
  }

  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const content::ConsoleMessage& message) override {
    if (scope != scope_)
      return;

    auto* worker_context =
        profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
    worker_context->RemoveObserver(this);

    // Shouldn't happen because we unregistered as observers.
    DCHECK(!message_.has_value());

    message_ = message.message;
    run_loop_.Quit();
  }

 private:
  Profile* const profile_;
  const GURL scope_;

  absl::optional<std::u16string> message_;
  base::RunLoop run_loop_;
};

base::FilePath GetWindowManagerExtensionDir() {
  base::FilePath test_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  return test_dir.Append("system_extensions")
      .Append("window_manager_extension");
}

class CrosWindowBrowserTest : public InProcessBrowserTest {
 public:
  CrosWindowBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSystemExtensions);

    installation_ =
        web_app::TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
  ~CrosWindowBrowserTest() override = default;

  // TODO(b/210737979):
  // Remove switch toggles when service workers are supported on
  // chrome-untrusted://
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kSystemExtensionsDebug);
    command_line->AppendSwitchASCII(
        ::switches::kEnableBlinkFeatures,
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

class CrosWindowExtensionBrowserTest : public InProcessBrowserTest {
 public:
  CrosWindowExtensionBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kSystemExtensions,
         ::features::kEnableServiceWorkersForChromeUntrusted},
        {});
  }

  ~CrosWindowExtensionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    debug_observer_ = std::make_unique<DebugServiceWorkerContextObserver>(
        browser()->profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DebugServiceWorkerContextObserver> debug_observer_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowMoveTo) {
  const char test_code[] = R"(
async function cros_test() {
  let [window] = await chromeos.windowManagement.getWindows();

  let x = window.screenLeft;
  let y = window.screenTop;
  x -= 20;
  y -= 20;

  await moveToAndTest(x, y);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowMoveBy) {
  const char test_code[] = R"(
async function cros_test() {
  let [window] = await chromeos.windowManagement.getWindows();

  await moveByAndTest(-20, -20);

  // Check that calling twice continues to move the window.
  await moveByAndTest(10, 10);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowResizeTo) {
  const char test_code[] = R"(
async function cros_test() {
  let [window] = await chromeos.windowManagement.getWindows();

  let width = window.width;
  let height = window.height;
  width -= 20;
  height -= 20;

  await resizeToAndTest(width, height);
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosWindowResizeBy) {
  const char test_code[] = R"(
async function cros_test() {
  let [window] = await chromeos.windowManagement.getWindows();

  await resizeByAndTest(-20, -20);

  await resizeByAndTest(10, 10);
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
    await first_window.focus();
  }

  {
    let [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // When focusing 2nd window, it should have sole focus.
    await second_window.focus();

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Fullscreening a window does not focus an unfocused window.
    await first_window.setFullscreen(true);

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // We can focus a fullscreen window.
    await first_window.focus();

    [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // We can focus another window on top of a fullscreen window.
    await second_window.focus();

    [first_window, second_window] = await getWindows();
    assert_false(first_window.isFocused);
    assert_true(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Minimizing focused window should pass focus to next window.
    await second_window.minimize();

    [first_window, second_window] = await getWindows();
    assert_true(first_window.isFocused);
    assert_false(second_window.isFocused);
  }

  {
    let [first_window, second_window] = await getWindows();
    // Minimizing remaining window should lose focus.
    await first_window.minimize();

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

  await swa_window.minimize();
  await swa_window.focus();
  await swa_window.maximize();
  await swa_window.setFullscreen(true);
  await swa_window.close();
}
  )",
                                             target_id.c_str());

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest,
                       CrosWindowPendingCallsToGetAllWindowsShouldNotCrash) {
  const char test_code[] = R"(
async function cros_test() {
  let getWindowsPromise = chromeos.windowManagement.getWindows();
  for (let i = 0; i < 100; i++)
    chromeos.windowManagement.getWindows();
  await getWindowsPromise;
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest,
                       CrosWindowPendingCallsToGetWindowShouldNotCrash) {
  const char test_code[] = R"(
async function cros_test() {
  let [window] = await chromeos.windowManagement.getWindows();
  let movePromise = window.moveTo(0, 0);
  for (let i = 0; i < 100; i++)
    window.moveTo(0, 0);
  await movePromise;
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest,
                       CrosWindowPendingCallsToGetWidgetShouldNotCrash) {
  const char test_code[] = R"(
async function cros_test() {
  let [window] = await chromeos.windowManagement.getWindows();
  let fullscreenPromise = window.setFullscreen(true);
  for (let i = 0; i < 100; i++)
    window.setFullscreen(true);
  await fullscreenPromise;
}
  )";

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

IN_PROC_BROWSER_TEST_F(CrosWindowBrowserTest, CrosAcceleratorEventIdl) {
  const char test_code[] = R"(
async function cros_test() {
  assert_true(chromeos.CrosAcceleratorEvent !== undefined, 'event');
  let accelerator_event = new chromeos.CrosAcceleratorEvent(
     'acceleratordown', {acceleratorName: 'close-window', repeat: false});
  assert_equals(accelerator_event.type, 'acceleratordown', 'event type');
  assert_equals(accelerator_event.acceleratorName, 'close-window');
  assert_false(accelerator_event.repeat);
  assert_true(accelerator_event.bubbles, 'bubbles');
  assert_false(accelerator_event.cancelable, 'cancelable');
}
  )";

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowExtensionBrowserTest, StartEvent) {
  auto* provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider->install_manager();

  // TODO(b/230811571): Rather than using the console to wait for the
  // observer to get called, we should add support for running async functions
  // to content::ServiceWorkerContext::ExecuteScriptForTest.
  ServiceWorkerConsoleObserver sw_console_observer(
      browser()->profile(),
      GURL("chrome-untrusted://system-extension-echo-01020304/"));

  base::RunLoop run_loop;
  LOG(ERROR) << "Starting installation.";
  install_manager.InstallUnpackedExtensionFromDir(
      GetWindowManagerExtensionDir(),
      base::BindLambdaForTesting([&](InstallStatusOrSystemExtensionId result) {
        ASSERT_TRUE(result.ok());
        ASSERT_EQ(kTestSystemExtensionId, result.value());
        run_loop.Quit();
      }));
  run_loop.Run();
  LOG(ERROR) << "Installation finished.";

  EXPECT_EQ(u"start event fired",
            sw_console_observer.WaitAndGetNextConsoleMessage());
}

}  //  namespace ash
