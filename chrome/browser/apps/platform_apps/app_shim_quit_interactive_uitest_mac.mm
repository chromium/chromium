// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests behavior when quitting apps with app shims.

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>
#include <unistd.h>

#include <vector>

#include "apps/switches.h"
#include "base/functional/bind.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/events/test/cocoa_test_event_utils.h"

using extensions::PlatformAppBrowserTest;

namespace apps {

namespace {

// Test class used to expose protected methods of AppShimHostBootstrap.
class TestAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  TestAppShimHostBootstrap() : AppShimHostBootstrap(getpid()) {}
  TestAppShimHostBootstrap(const TestAppShimHostBootstrap&) = delete;
  TestAppShimHostBootstrap& operator=(const TestAppShimHostBootstrap&) = delete;
  using AppShimHostBootstrap::OnShimConnected;
};

// Starts an app without a browser window using --load_and_launch_app and
// --silent_launch.
class AppShimQuitTest : public PlatformAppBrowserTest {
  AppShimQuitTest(const AppShimQuitTest&) = delete;
  AppShimQuitTest& operator=(const AppShimQuitTest&) = delete;

 protected:
  AppShimQuitTest() = default;

  void SetUpAppShim() {
    ASSERT_EQ(0u, [[NSApp windows] count]);
    ExtensionTestMessageListener launched_listener("Launched");
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
    ASSERT_EQ(1u, [[NSApp windows] count]);

    manager_ = g_browser_process->platform_part()->app_shim_manager();

    // Attach a host for the app.
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile());
    extension_id_ =
        GetExtensionByPath(registry->enabled_extensions(), app_path_)->id();
    mojo::Remote<chrome::mojom::AppShimHost> host;
    auto app_shim_info = chrome::mojom::AppShimInfo::New();
    app_shim_info->profile_path = profile()->GetBaseName();
    app_shim_info->app_id = extension_id_;
    app_shim_info->app_url = GURL("https://example.com");
    app_shim_info->launch_type =
        chrome::mojom::AppShimLaunchType::kRegisterOnly;
    (new TestAppShimHostBootstrap)
        ->OnShimConnected(host.BindNewPipeAndPassReceiver(),
                          std::move(app_shim_info),
                          base::BindOnce(&AppShimQuitTest::DoShimLaunchDone,
                                         base::Unretained(this)));

    // Focus the app window.
    NSWindow* window = [[NSApp windows] objectAtIndex:0];
    EXPECT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(window));
    content::RunAllPendingInMessageLoop();
  }

  void DoShimLaunchDone(
      chrome::mojom::AppShimLaunchResult result,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    // Simulate an app shim initiated launch, i.e. launch app but not browser.
    app_path_ =
        test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal");
    command_line->AppendSwitchNative(apps::kLoadAndLaunchApp,
                                     app_path_.value());
    command_line->AppendSwitch(switches::kSilentLaunch);
  }

  base::FilePath app_path_;
  raw_ptr<AppShimManager, DanglingUntriaged> manager_ = nullptr;
  std::string extension_id_;
};

}  // namespace

// Test that closing an app with Cmd+Q when no browsers have ever been open
// terminates Chrome.
IN_PROC_BROWSER_TEST_F(AppShimQuitTest, QuitWithKeyEvent) {
  SetUpAppShim();

  // Simulate a Cmd+Q event.
  NSWindow* window = [[NSApp windows] objectAtIndex:0];
  NSEvent* event = cocoa_test_event_utils::KeyEventWithKeyCode(
      0, 'q', NSEventTypeKeyDown, NSEventModifierFlagCommand);
  [window postEvent:event atStart:NO];

  // This will time out if the event above does not terminate Chrome.
  RunUntilBrowserProcessQuits();

  EXPECT_FALSE(manager_->FindHost(profile(), extension_id_));
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
}

}  // namespace apps
