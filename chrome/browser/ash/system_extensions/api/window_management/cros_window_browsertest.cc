// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_extensions/api/test_support/system_extensions_api_browsertest.h"
#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_test_helper.test-mojom.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

static constexpr const char kTestsDir[] =
    "chrome/browser/ash/system_extensions/api/window_management/test";
static constexpr const char kManifestTemplate[] = R"(
{
  "name": "Test Window Manager Extension",
  "short_name": "Test",
  "service_worker_url": "/%s",
  "id": "01020304",
  "type": "window-management"
})";

Profile* GetProfile() {
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(active_user);
  auto* profile = ProfileHelper::Get()->GetProfileByUser(active_user);
  DCHECK(profile);
  return profile;
}

base::UnguessableToken GetUnguessableToken(base::StringPiece str) {
  absl::optional<base::Token> token = base::Token::FromString(str);
  DCHECK(token.has_value());

  absl::optional<base::UnguessableToken> unguessable_token =
      base::UnguessableToken::Deserialize(token->high(), token->low());
  return unguessable_token.value();
}

// Class used to wait for InstanceRegistry events.
class InstanceRegistryEventWaiter : public apps::InstanceRegistry::Observer {
 public:
  InstanceRegistryEventWaiter() {
    instance_registry_observation_.Observe(
        &apps::AppServiceProxyFactory::GetForProfile(GetProfile())
             ->InstanceRegistry());
  }

  ~InstanceRegistryEventWaiter() override = default;

  // Returns the id of the next window that gets created.
  base::UnguessableToken WaitForCreation() {
    run_loop_.Run();
    return window_id_.value();
  }

  // apps::InstanceRegistry::Observer
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override {
    if (!update.IsCreation())
      return;

    window_id_ = update.InstanceId();
    instance_registry_observation_.Reset();
    run_loop_.Quit();
  }

  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override {
    // This class is created and destroyed during tests so it will always be
    // destroyed by the time the InstanceRegistry is destroyed.
    NOTREACHED();
  }

 private:
  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      instance_registry_observation_{this};

  // This waiter is used from within Mojo method implementations which would
  // result in nested run loops which would hang the test. Allow nestable tasks
  // to get around this.
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  absl::optional<base::UnguessableToken> window_id_;
};

// Class used by tests to perform actions on the browser side.
class CrosWindowManagementTestHelper
    : public system_extensions_test::mojom::CrosWindowManagementTestHelper {
 public:
  explicit CrosWindowManagementTestHelper(
      TestSystemWebAppInstallation& swa_installation)
      : swa_installation_(swa_installation) {}
  ~CrosWindowManagementTestHelper() override = default;

  void OpenSystemWebAppWindow(
      OpenSystemWebAppWindowCallback callback) override {
    InstanceRegistryEventWaiter waiter;
    ash::LaunchSystemWebAppAsync(GetProfile(), swa_installation_->GetType());
    base::UnguessableToken window_id = waiter.WaitForCreation();
    std::move(callback).Run(window_id.ToString());
  }

  void OpenBrowserWindow(OpenBrowserWindowCallback callback) override {
    InstanceRegistryEventWaiter waiter;
    chrome::NewEmptyWindow(GetProfile());
    base::UnguessableToken window_id = waiter.WaitForCreation();
    std::move(callback).Run(window_id.ToString());
  }

  void CloseBrowserWindow(const std::string& id,
                          CloseBrowserWindowCallback callback) override {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(GetProfile());
    CHECK(proxy->InstanceRegistry().ForOneInstance(
        GetUnguessableToken(id),
        [callback =
             std::move(callback)](const apps::InstanceUpdate& update) mutable {
          Browser* browser = chrome::FindBrowserWithWindow(
              update.Window()->GetToplevelWindow());
          CHECK(browser);

          chrome::CloseWindow(browser);
          std::move(callback).Run();
        }));
  }

  void SetDisplays(const std::string& displays,
                   SetDisplaysCallback callback) override {
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay(displays);

    std::move(callback).Run();
  }

  void GetMinimumSize(const std::string& id,
                      GetMinimumSizeCallback callback) override {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(GetProfile());
    CHECK(proxy->InstanceRegistry().ForOneInstance(
        GetUnguessableToken(id),
        [callback =
             std::move(callback)](const apps::InstanceUpdate& update) mutable {
          std::move(callback).Run(update.Window()
                                      ->GetToplevelWindow()
                                      ->delegate()
                                      ->GetMinimumSize());
        }));
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

 private:
  raw_ref<TestSystemWebAppInstallation, DanglingUntriaged> swa_installation_;
};

class CrosWindowManagementBrowserTest : public SystemExtensionsApiBrowserTest {
 public:
  CrosWindowManagementBrowserTest()
      : SystemExtensionsApiBrowserTest(
            {.tests_dir = kTestsDir,
             .manifest_template = kManifestTemplate,
             .additional_src_files = {"chrome/test/data/system_extensions/"
                                      "cros_window_test_utils.js"},
             .additional_gen_files = {
                 "chrome/browser/ash/system_extensions/api/"
                 "window_management/"
                 "cros_window_management_test_helper.test-mojom-lite.js",
                 "ui/events/mojom/keyboard_codes.mojom-lite.js",
                 "ui/events/mojom/event_constants.mojom-lite.js",
                 "ui/gfx/geometry/mojom/geometry.mojom-lite.js",
             }}) {
    installation_ =
        TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();

    AddRendererInterface(base::BindLambdaForTesting(
        [this](mojo::PendingReceiver<
               system_extensions_test::mojom::CrosWindowManagementTestHelper>
                   pending_receiver) {
          auto test_helper = std::make_unique<CrosWindowManagementTestHelper>(
              this->swa_installation());
          this->test_helpers_.Add(std::move(test_helper),
                                  std::move(pending_receiver));
        }));

    // Needed because otherwise ChromeOS remaps some accelerators e.g.
    // to other keys e.g. "Alt + Arrow up" to "Home".
    feature_list_.InitAndEnableFeature(::features::kDeprecateAltBasedSixPack);
  }

 protected:
  TestSystemWebAppInstallation& swa_installation() { return *installation_; }

  // Installs and launches a Web App that opens in a tab.
  void InstallAndLaunchBrowserWebApp() {
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL start_url =
        embedded_test_server()->GetURL("/web_apps/basic.html");

    // Install a web app with `browser` display mode, so that it launched
    // in a tab in regular browser window.
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = start_url;
    web_app_info->user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;
    const web_app::AppId app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    web_app::AppReadinessWaiter(browser()->profile(), app_id).Await();

    // Launch app through App Service proxy and wait for it to open.
    auto* const proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());

    ui_test_utils::TabAddedWaiter waiter(browser());
    proxy->Launch(app_id,
                  /*event_flags=*/0, apps::LaunchSource::kFromAppListGrid);
    waiter.Wait();
  }

 private:
  mojo::UniqueReceiverSet<
      system_extensions_test::mojom::CrosWindowManagementTestHelper>
      test_helpers_;
  std::unique_ptr<TestSystemWebAppInstallation> installation_;

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
                       CrosWindowResizeBy_OverrideMinimumSize) {
  RunTest("cros_window_resize_by_override_minimum_size.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosWindowResizeTo_OverrideMinimumSize) {
  RunTest("cros_window_resize_to_override_minimum_size.js");
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

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowRestore) {
  RunTest("cros_window_restore.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosWindowRestoreWithHistory) {
  RunTest("cros_window_restore_with_history.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowFocusSingle) {
  RunTest("cros_window_focus_single.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowFocusMulti) {
  RunTest("cros_window_focus_multi.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowClose) {
  RunTest("cros_window_close.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, WindowOpenedEvent) {
  RunTest("window_opened_event.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, CrosWindowWebAppTab) {
  InstallAndLaunchBrowserWebApp();

  // Unfocus the web app.
  chrome::SelectPreviousTab(browser());

  // Run test which calls .getWindows().
  RunTest("cros_window_web_app_tab.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, IgnoreWebAppTabs) {
  InstallAndLaunchBrowserWebApp();

  RunTest("cros_ignore_web_app_tabs.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CacheGetWindowsReturnsProperty) {
  RunTest("cache_get_windows_returns_property.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CrosWindowSWACrashTest) {
  // Calling this from inside a Mojo method implementation causes a nested loop
  // and the test hangs. Call it here instead.
  swa_installation().WaitForAppInstall();
  RunTest("cros_window_swa_crash_test.js");
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

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest, StartEvent) {
  RunTest("cros_start_event.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       CloseOutsideOfExtension) {
  RunTest("close_outside_of_extension.js");
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

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       AcceleratorEvent_ArrowKeys) {
  RunTest("accelerator_event_arrow_keys.js");
}

IN_PROC_BROWSER_TEST_F(CrosWindowManagementBrowserTest,
                       AcceleratorEvent_WakeUpWorker) {
  InitMultiRunTest("accelerator_event_wake_up_sw.js");
  WaitForRun("First run");

  StopServiceWorkers();

  // Dispatch event. The Service Worker should wake up and fire an event.
  ui::test::EventGenerator generator(ash::Shell::Get()->GetPrimaryRootWindow());
  generator.PressKey(ui::KeyboardCode::VKEY_A,
                     ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);

  WaitForRun("Test event properties");
}

}  //  namespace ash
