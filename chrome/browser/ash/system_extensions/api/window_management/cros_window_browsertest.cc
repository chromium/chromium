// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/system_extensions/api/test_support/system_extensions_api_browsertest.h"
#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_test_helper.test-mojom.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/aura/window.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"

namespace ash {
class AshTestBase;
class Shelf;
class ShelfWidget;

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
    if (!message_.has_value())
      run_loop_.Run();

    return message_.value();
  }

  base::Value WaitAndGetNextConsoleMessageAsValue() {
    std::string result = base::UTF16ToUTF8(WaitAndGetNextConsoleMessage());
    return *base::JSONReader::Read(result);
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

static constexpr const char kTestsDir[] =
    "chrome/browser/ash/system_extensions/api/window_management/test";
static constexpr const char kManifestTemplate[] = R"(
{
  "name": "Test Window Manager Extension",
  "short_name": "Test",
  "service_worker_url": "/%s",
  "id": "01020304",
  "type": "echo"
})";

class CrosWindowManagementTestHelper
    : public system_extensions_test::mojom::CrosWindowManagementTestHelper {
 public:
  CrosWindowManagementTestHelper() = default;
  ~CrosWindowManagementTestHelper() override = default;

  void SetDisplays(const std::string& displays,
                   SetDisplaysCallback callback) override {
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay(displays);

    std::move(callback).Run();
  }

  void GetShelfHeight(GetShelfHeightCallback callback) override {
    gfx::Rect shelf_bounds =
        AshTestBase::GetPrimaryShelf()->shelf_widget()->GetVisibleShelfBounds();

    std::move(callback).Run(shelf_bounds.height());
  }

  void SimulatePressKey(ui::mojom::KeyboardCode keyboard_code,
                        int32_t modifiers,
                        SimulatePressKeyCallback callback) override {
    ui::test::EventGenerator generator(
        ash::Shell::Get()->GetPrimaryRootWindow());
    generator.PressKey(static_cast<ui::KeyboardCode>(keyboard_code), modifiers);

    std::move(callback).Run();
  }

  void SimulateReleaseKey(ui::mojom::KeyboardCode keyboard_code,
                          int32_t modifiers,
                          SimulateReleaseKeyCallback callback) override {
    ui::test::EventGenerator generator(
        ash::Shell::Get()->GetPrimaryRootWindow());
    generator.ReleaseKey(static_cast<ui::KeyboardCode>(keyboard_code),
                         modifiers);

    std::move(callback).Run();
  }
};

class CrosWindowManagementBrowserTest : public SystemExtensionsApiBrowserTest {
 public:
  CrosWindowManagementBrowserTest()
      : SystemExtensionsApiBrowserTest({
            .tests_dir = kTestsDir,
            .manifest_template = kManifestTemplate,
            .additional_src_files = {"chrome/test/data/system_extensions/"
                                     "cros_window_test_utils.js"},
            .additional_gen_files =
                {"gen/chrome/browser/ash/system_extensions/api/"
                 "window_management/"
                 "cros_window_management_test_helper.test-mojom-lite.js",
                 "gen/ui/events/mojom/keyboard_codes.mojom-lite.js",
                 "gen/ui/events/mojom/event_constants.mojom-lite.js"},
        }) {
    AddRendererInterface(base::BindLambdaForTesting(
        [](mojo::PendingReceiver<
            system_extensions_test::mojom::CrosWindowManagementTestHelper>
               pending_receiver) {
          mojo::MakeSelfOwnedReceiver(
              std::make_unique<CrosWindowManagementTestHelper>(),
              std::move(pending_receiver));
        }));
  }
};

// Deprecated. Use CrosWindowManagementBrowserTest instead.
// TODO(b/242264794): Remove once all tests are migrated to
// CrosWindowManagementBrowserTest.
class CrosWindowLegacyBrowserTest : public InProcessBrowserTest {
 public:
  CrosWindowLegacyBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSystemExtensions);

    installation_ =
        TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
  ~CrosWindowLegacyBrowserTest() override = default;

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

 protected:
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

  std::unique_ptr<TestSystemWebAppInstallation> installation_;

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

  void InstallSystemExtension() {
    auto& provider = SystemExtensionsProvider::Get(browser()->profile());
    auto& install_manager = provider.install_manager();

    base::RunLoop run_loop;
    install_manager.InstallUnpackedExtensionFromDir(
        GetWindowManagerExtensionDir(),
        base::BindLambdaForTesting(
            [&](InstallStatusOrSystemExtensionId result) {
              ASSERT_TRUE(result.ok());
              ASSERT_EQ(kTestSystemExtensionId, result.value());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void InstallAndStartExtension() {
    ServiceWorkerConsoleObserver sw_console_observer(
        browser()->profile(),
        GURL("chrome-untrusted://system-extension-echo-01020304/"));

    InstallSystemExtension();

    ASSERT_EQ(u"start event fired",
              sw_console_observer.WaitAndGetNextConsoleMessage());
  }

  ServiceWorkerConsoleObserver GetConsoleObserver() {
    return ServiceWorkerConsoleObserver(
        browser()->profile(),
        GURL("chrome-untrusted://system-extension-echo-01020304/"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosScreenProperties) {
  RunTest("cros_screen_properties.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowMoveTo) {
  RunTest("cros_window_move_to.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowMoveBy) {
  RunTest("cros_window_move_by.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowResizeTo) {
  RunTest("cros_window_resize_to.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowResizeBy) {
  RunTest("cros_window_resize_by.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosWindowSetFullscreen) {
  RunTest("cros_window_set_fullscreen.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, FullScreenMinMax) {
  RunTest("fullscreen_min_max.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       UnsetFullscreenNonMinimized) {
  RunTest("unset_fullscreen_non_minimized.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       UnsetFullscreenMinimized) {
  RunTest("unset_fullscreen_minimized.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowMaximize) {
  RunTest("cros_window_maximize.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowMinimize) {
  RunTest("cros_window_minimize.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowFocusSingle) {
  RunTest("cros_window_focus_single.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowFocusMulti) {
  // Open browser instance to take focus.
  chrome::NewWindow(browser());

  RunTest("cros_window_focus_multi.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowLegacyBrowserTest, CrosWindowClose) {
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

  // TODO(b/242264794): Events are only dispatched to system extensions. Since
  // this test doesn't use an actual System Extension, the commented out code
  // below hangs. Uncomment once this test moves to running in a System
  // Extension.
  // let promise = eventPromise(chromeos.windowManagement, 'windowclosed');
  // window_to_close.close();
  // let e = await promise;
  // assert_equals(e.window.id, "%1$s");

  // windows = await chromeos.windowManagement.getWindows();
  // assert_equals(windows.length, 1);
}
  )",
                                             target_id.c_str());

  RunTest(test_code);
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CacheGetWindowsReturnsProperty) {
  RunTest("cache_get_windows_returns_property.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowLegacyBrowserTest, CrosWindowSWACrashTest) {
  // Finish installation of Sample SWA.
  installation_->WaitForAppInstall();

  // Wait for Sample SWA window to open.
  content::TestNavigationObserver navigation_observer(
      installation_->GetAppUrl());
  navigation_observer.StartWatchingNewWebContents();

  ash::LaunchSystemWebAppAsync(browser()->profile(), installation_->GetType());

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

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosWindowPendingCallsToGetAllWindowsShouldNotCrash) {
  RunTest("cros_window_pending_calls_to_get_all_windows_should_not_crash.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosWindowPendingCallsToGetWindowShouldNotCrash) {
  RunTest("cros_window_pending_calls_to_get_window_should_not_crash.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosWindowPendingCallsToGetWidgetShouldNotCrash) {
  RunTest("cros_window_pending_calls_to_get_widget_should_not_crash.js");
}

// Tests that the CrosWindowManagement object is an EventTarget.
IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosWindowManagementEventTarget) {
  RunTest("cros_window_manager_event_target.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosAcceleratorEventIdl) {
  RunTest("cros_accelerator_event_idl.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowEventIdl) {
  RunTest("cros_window_event_idl.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowExtensionBrowserTest, StartEvent) {
  // TODO(b/230811571): Rather than using the console to wait for the
  // observer to get called, we should add support for running async functions
  // to content::ServiceWorkerContext::ExecuteScriptForTest.
  auto sw_console_observer = GetConsoleObserver();
  InstallSystemExtension();
  EXPECT_EQ(u"start event fired",
            sw_console_observer.WaitAndGetNextConsoleMessage());
}

IN_PROC_BROWSER_TEST_F(CrosWindowExtensionBrowserTest, CloseEvent) {
  InstallAndStartExtension();

  // Open browser instance to close outside of service worker.
  chrome::NewWindow(browser());

  // Keep track of the new browser window so we can close it.
  Browser* new_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_NE(browser(), new_browser);

  // Set target id to crosWindow id of newly opened window as per instance
  // registry.
  std::string target_id;

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->InstanceRegistry().ForEachInstance(
      [&target_id, &new_browser](const apps::InstanceUpdate& update) {
        if (update.Window()->GetToplevelWindow() ==
            new_browser->window()->GetNativeWindow()) {
          CHECK(target_id.empty());
          target_id = update.InstanceId().ToString();
        }
      });

  auto observer = GetConsoleObserver();

  chrome::CloseWindow(new_browser);

  base::Value result = observer.WaitAndGetNextConsoleMessageAsValue();

  // Our event should be dispatched with our system extension logging the id of
  // the window firing the event.
  EXPECT_EQ(result, target_id);
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, AcceleratorEvent) {
  RunTest("accelerator_event.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       AcceleratorEvent_Repeat) {
  RunTest("accelerator_event_repeat.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       AcceleratorEvent_NoEvent) {
  RunTest("accelerator_event_no_event.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       AcceleratorEvent_Shift) {
  RunTest("accelerator_event_shift.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       AcceleratorEvent_ReleaseKey) {
  RunTest("accelerator_event_release_key.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowExtensionBrowserTest,
                       AcceleratorEvent_WakeUpWorker) {
  InstallAndStartExtension();

  // Stop the Service Worker.
  auto* worker_context = browser()
                             ->profile()
                             ->GetDefaultStoragePartition()
                             ->GetServiceWorkerContext();
  base::RunLoop run_loop;
  worker_context->StopAllServiceWorkers(run_loop.QuitClosure());
  run_loop.Run();

  // Dispatch event. The Service Worker should wake up and fire an event.
  auto observer = GetConsoleObserver();
  ui::test::EventGenerator generator(ash::Shell::Get()->GetPrimaryRootWindow());
  generator.PressKey(ui::KeyboardCode::VKEY_A,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);

  base::Value result = observer.WaitAndGetNextConsoleMessageAsValue();
  const auto& dict = result.GetDict();

  EXPECT_EQ("acceleratordown", *dict.FindString("type"));
  EXPECT_EQ("Control Alt a", *dict.FindString("name"));
  EXPECT_FALSE(*dict.FindBool("repeat"));
}

}  //  namespace ash
