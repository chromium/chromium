// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <memory>
#include <utility>
#include <vector>

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/switches.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/mac/foundation_util.h"
#import "base/mac/launch_application.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_listener.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/apps/platform_apps/extension_app_shim_manager_delegate_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#import "ui/events/test/cocoa_test_event_utils.h"

using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

// General end-to-end test for app shims.
class AppShimInteractiveTest : public extensions::PlatformAppBrowserTest {
 public:
  AppShimInteractiveTest(const AppShimInteractiveTest&) = delete;
  AppShimInteractiveTest& operator=(const AppShimInteractiveTest&) = delete;

 protected:
  // Type of app to install, when invoking InstallAppWithShim().
  enum AppType { APP_TYPE_PACKAGED, APP_TYPE_HOSTED };

  AppShimInteractiveTest()
      : auto_reset_(&g_app_shims_allow_update_and_launch_in_tests, true) {}

  // Install a test app of |type| and reliably wait for its app shim to be
  // created on disk. Sets |shim_path_|.
  const extensions::Extension* InstallAppWithShim(AppType type,
                                                  const char* name);

 protected:
  base::FilePath shim_path_;

 private:
  // Temporarily enable app shims.
  base::AutoReset<bool> auto_reset_;
};

// Watches for an app shim to connect.
class WindowedAppShimLaunchObserver : public apps::AppShimManager {
 public:
  explicit WindowedAppShimLaunchObserver(const std::string& app_id)
      : AppShimManager(
            std::make_unique<apps::ExtensionAppShimManagerDelegate>()),
        app_mode_id_(app_id) {
    StartObserving();
  }
  WindowedAppShimLaunchObserver(const WindowedAppShimLaunchObserver&) = delete;
  WindowedAppShimLaunchObserver& operator=(
      const WindowedAppShimLaunchObserver&) = delete;

  void StartObserving() {
    observed_ = false;
    AppShimHostBootstrap::SetClient(this);
  }

  void Wait() {
    if (observed_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // AppShimHandler:
  void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override {
    AppShimManager::OnShimProcessConnected(std::move(bootstrap));
    observed_ = true;
    if (run_loop_.get())
      run_loop_->Quit();
  }

  // AppShimHost::Client:
  void OnShimLaunchRequested(
      AppShimHost* host,
      bool recreate_shims,
      apps::ShimLaunchedCallback launch_callback,
      apps::ShimTerminatedCallback terminated_callback) override {
    AppShimManager::OnShimLaunchRequested(host, recreate_shims,
                                          std::move(launch_callback),
                                          std::move(terminated_callback));
  }
  void OnShimProcessDisconnected(AppShimHost* host) override {
    AppShimManager::OnShimProcessDisconnected(host);
    observed_ = true;
    if (run_loop_.get())
      run_loop_->Quit();
  }
  void OnShimFocus(AppShimHost* host) override {}
  void OnShimReopen(AppShimHost* host) override {}
  void OnShimOpenedFiles(AppShimHost* host,
                         const std::vector<base::FilePath>& files) override {}
  void OnShimSelectedProfile(AppShimHost* host,
                             const base::FilePath& profile_path) override {}

 private:
  std::string app_mode_id_;
  bool observed_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Watches for a hosted app browser window to open.
class HostedAppBrowserListObserver : public BrowserListObserver {
 public:
  explicit HostedAppBrowserListObserver(const std::string& app_id)
      : app_id_(app_id) {
    BrowserList::AddObserver(this);
  }

  HostedAppBrowserListObserver(const HostedAppBrowserListObserver&) = delete;
  HostedAppBrowserListObserver& operator=(const HostedAppBrowserListObserver&) =
      delete;
  ~HostedAppBrowserListObserver() override {
    BrowserList::RemoveObserver(this);
  }

  void WaitUntilAdded() {
    if (observed_add_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void WaitUntilRemoved() {
    if (observed_removed_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override {
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id_) {
      observed_add_ = true;
      if (run_loop_.get())
        run_loop_->Quit();
    }
  }

  void OnBrowserRemoved(Browser* browser) override {
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id_) {
      observed_removed_ = true;
      if (run_loop_.get())
        run_loop_->Quit();
    }
  }

 private:
  std::string app_id_;
  bool observed_add_ = false;
  bool observed_removed_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class AppLifetimeMonitorObserver : public apps::AppLifetimeMonitor::Observer {
 public:
  explicit AppLifetimeMonitorObserver(Profile* profile) : profile_(profile) {
    apps::AppLifetimeMonitorFactory::GetForBrowserContext(profile_)
        ->AddObserver(this);
  }
  AppLifetimeMonitorObserver(const AppLifetimeMonitorObserver&) = delete;
  AppLifetimeMonitorObserver& operator=(const AppLifetimeMonitorObserver&) =
      delete;
  ~AppLifetimeMonitorObserver() override {
    apps::AppLifetimeMonitorFactory::GetForBrowserContext(profile_)
        ->RemoveObserver(this);
  }

  int activated_count() { return activated_count_; }
  int deactivated_count() { return deactivated_count_; }

 protected:
  // AppLifetimeMonitor::Observer overrides:
  void OnAppActivated(content::BrowserContext* context,
                      const std::string& app_id) override {
    ++activated_count_;
  }
  void OnAppDeactivated(content::BrowserContext* context,
                        const std::string& app_id) override {
    ++deactivated_count_;
  }

 private:
  raw_ptr<Profile> profile_;
  int activated_count_ = 0;
  int deactivated_count_ = 0;
};

NSString* GetBundleID(const base::FilePath& shim_path) {
  base::FilePath plist_path = shim_path.Append("Contents").Append("Info.plist");
  NSMutableDictionary* plist = [NSMutableDictionary
      dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
  return [plist objectForKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
}

bool HasAppShimHost(Profile* profile, const std::string& app_id) {
  return g_browser_process->platform_part()->app_shim_manager()->FindHost(
      profile, app_id);
}

base::FilePath GetAppShimPath(Profile* profile,
                              const extensions::Extension* app) {
  // Use a WebAppShortcutCreator to get the path.
  std::unique_ptr<web_app::ShortcutInfo> shortcut_info =
      web_app::ShortcutInfoForExtensionAndProfile(app, profile);
  web_app::WebAppShortcutCreator shortcut_creator(
      web_app::GetOsIntegrationResourcesDirectoryForApp(profile->GetPath(),
                                                        app->id(), GURL()),
      shortcut_info.get());
  return shortcut_creator.GetApplicationsShortcutPath(false);
}

Browser* GetFirstHostedAppWindow() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    const std::string app_id =
        web_app::GetAppIdFromApplicationName(browser->app_name());
    ExtensionRegistry* registry = ExtensionRegistry::Get(browser->profile());
    const Extension* extension = registry->enabled_extensions().GetByID(app_id);
    if (extension && extension->is_hosted_app())
      return browser;
  }
  return nullptr;
}

const extensions::Extension* AppShimInteractiveTest::InstallAppWithShim(
    AppType type,
    const char* name) {
  const extensions::Extension* app;
  switch (type) {
    case APP_TYPE_PACKAGED:
      app = InstallPlatformApp(name);
      break;
    case APP_TYPE_HOSTED:
      app = InstallHostedApp();
      break;
  }

  // Note that usually an install triggers shim creation, but that's disabled
  // (always) in tests. If it wasn't the case, the following test would fail
  // (but flakily since the creation happens on the FILE thread).
  base::ScopedAllowBlockingForTesting allow_blocking;
  shim_path_ = GetAppShimPath(profile(), app);
  EXPECT_FALSE(base::PathExists(shim_path_));

  // To create a shim in a test, instead call UpdateAllShortcuts, which has been
  // blessed by g_app_shims_allow_update_and_launch_in_tests.
  base::RunLoop run_loop;
  web_app::UpdateAllShortcuts(std::u16string(), profile(), app,
                              run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(base::PathExists(shim_path_));
  return app;
}

}  // namespace

namespace apps {

// Shims require static libraries unless running from their original build
// location.
// https://crbug.com/386024, https://crrev.com/619648
#if defined(COMPONENT_BUILD)
#define MAYBE_Launch DISABLED_Launch
#define MAYBE_HostedAppLaunch DISABLED_HostedAppLaunch
#define MAYBE_ShowWindow DISABLED_ShowWindow
#define MAYBE_RebuildShim DISABLED_RebuildShim
#else
#define MAYBE_Launch DISABLED_Launch  // http://crbug.com/913490
#define MAYBE_HostedAppLaunch DISABLED_HostedAppLaunch
#define MAYBE_ShowWindow DISABLED_ShowWindow  // https://crbug.com/980072
// http://crbug.com/517744 HostedAppLaunch fails with open as tab for apps
// http://crbug.com/509774 this test is flaky so is disabled even in the
// static build.
#define MAYBE_RebuildShim DISABLED_RebuildShim
#endif

IN_PROC_BROWSER_TEST_F(AppShimInteractiveTest, MAYBE_HostedAppLaunch) {
  const extensions::Extension* app = InstallAppWithShim(APP_TYPE_HOSTED, "");
  NSString* bundle_id = GetBundleID(shim_path_);

  // Explicitly set the launch type to open in a new window.
  extensions::SetLaunchType(profile(), app->id(),
                            extensions::LAUNCH_TYPE_WINDOW);

  // Case 1: Launch the hosted app, it should start the shim.
  {
    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer;
    ns_observer.reset([[WindowedNSNotificationObserver alloc]
        initForWorkspaceNotification:NSWorkspaceDidLaunchApplicationNotification
                            bundleId:bundle_id]);
    WindowedAppShimLaunchObserver observer(app->id());
    LaunchHostedApp(app);
    EXPECT_TRUE([ns_observer wait]);
    observer.Wait();

    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));
    EXPECT_TRUE(GetFirstHostedAppWindow());

    NSArray* running_shim = [NSRunningApplication
        runningApplicationsWithBundleIdentifier:bundle_id];
    ASSERT_EQ(1u, [running_shim count]);

    ns_observer.reset([[WindowedNSNotificationObserver alloc]
        initForWorkspaceNotification:
            NSWorkspaceDidTerminateApplicationNotification
                            bundleId:bundle_id]);
    [base::mac::ObjCCastStrict<NSRunningApplication>(
        [running_shim objectAtIndex:0]) terminate];
    EXPECT_TRUE([ns_observer wait]);

    EXPECT_FALSE(GetFirstHostedAppWindow());
    EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  }

  // Case 2: Launch the shim, it should start the hosted app.
  {
    HostedAppBrowserListObserver listener(app->id());
    base::CommandLine shim_cmdline(base::CommandLine::NO_PROGRAM);
    shim_cmdline.AppendSwitch(app_mode::kLaunchedForTest);

    base::test::TestFuture<base::expected<NSRunningApplication*, NSError*>>
        open_result;
    base::mac::LaunchApplication(shim_path_, shim_cmdline, /*url_specs=*/{},
                                 /*options=*/{}, open_result.GetCallback());
    ASSERT_TRUE(open_result.Get<0>().has_value());
    NSRunningApplication* shim_app = open_result.Get<0>().value();
    ASSERT_TRUE(shim_app);

    base::Process shim_process(shim_app.processIdentifier);
    listener.WaitUntilAdded();

    ASSERT_TRUE(GetFirstHostedAppWindow());
    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));

    // If the window is closed, the shim should quit.
    GetFirstHostedAppWindow()->window()->Close();
    // Wait for the window to be closed.
    listener.WaitUntilRemoved();
    int exit_code;
    ASSERT_TRUE(shim_process.WaitForExitWithTimeout(
        TestTimeouts::action_timeout(), &exit_code));

    EXPECT_FALSE(GetFirstHostedAppWindow());
    EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  }
}

// Test that launching the shim for an app starts the app, and vice versa.
// These two cases are combined because the time to run the test is dominated
// by loading the extension and creating the shim.
IN_PROC_BROWSER_TEST_F(AppShimInteractiveTest, MAYBE_Launch) {
  const extensions::Extension* app =
      InstallAppWithShim(APP_TYPE_PACKAGED, "minimal");
  NSString* bundle_id = GetBundleID(shim_path_);

  // Case 1: Launch the app, it should start the shim.
  {
    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer;
    ns_observer.reset([[WindowedNSNotificationObserver alloc]
        initForWorkspaceNotification:NSWorkspaceDidLaunchApplicationNotification
                            bundleId:bundle_id]);
    WindowedAppShimLaunchObserver observer(app->id());
    LaunchPlatformApp(app);
    EXPECT_TRUE([ns_observer wait]);
    observer.Wait();

    EXPECT_TRUE(GetFirstAppWindow());
    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));

    // Quitting the shim will eventually cause it to quit. It actually
    // intercepts the -terminate, sends an QuitApp message to Chrome, and then
    // immediately quits. Chrome closes all windows of the app when QuitApp is
    // received.
    NSArray* running_shim = [NSRunningApplication
        runningApplicationsWithBundleIdentifier:bundle_id];
    ASSERT_EQ(1u, [running_shim count]);

    ns_observer.reset([[WindowedNSNotificationObserver alloc]
        initForWorkspaceNotification:
            NSWorkspaceDidTerminateApplicationNotification
                            bundleId:bundle_id]);
    observer.StartObserving();
    [base::mac::ObjCCastStrict<NSRunningApplication>(
        [running_shim objectAtIndex:0]) terminate];
    observer.Wait();
    EXPECT_TRUE([ns_observer wait]);

    EXPECT_FALSE(GetFirstAppWindow());
    EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  }

  // Case 2: Launch the shim, it should start the app.
  {
    ExtensionTestMessageListener launched_listener("Launched");
    base::CommandLine shim_cmdline(base::CommandLine::NO_PROGRAM);
    shim_cmdline.AppendSwitch(app_mode::kLaunchedForTest);

    base::test::TestFuture<base::expected<NSRunningApplication*, NSError*>>
        open_result;
    base::mac::LaunchApplication(shim_path_, shim_cmdline, /*url_specs=*/{},
                                 /*options=*/{}, open_result.GetCallback());
    ASSERT_TRUE(open_result.Get<0>().has_value());
    NSRunningApplication* shim_app = open_result.Get<0>().value();
    ASSERT_TRUE(shim_app);

    base::Process shim_process(shim_app.processIdentifier);
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

    ASSERT_TRUE(GetFirstAppWindow());
    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));

    // If the window is closed, the shim should quit.
    // Closing the window in views requires this thread to process tasks as the
    // final request to close the window is posted to this thread's queue.
    GetFirstAppWindow()->GetBaseWindow()->Close();
    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](base::Process* shim_process) {
              base::ScopedAllowBaseSyncPrimitivesForTesting
                  allow_base_sync_primitives;
              int exit_code;
              ASSERT_TRUE(shim_process->WaitForExitWithTimeout(
                  TestTimeouts::action_timeout(), &exit_code));
            },
            base::Unretained(&shim_process)),
        run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_FALSE(GetFirstAppWindow());
    EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  }
}

// Test that the shim's lifetime depends on the visibility of windows. I.e. the
// shim is only active when there are visible windows.
IN_PROC_BROWSER_TEST_F(AppShimInteractiveTest, MAYBE_ShowWindow) {
  const extensions::Extension* app =
      InstallAppWithShim(APP_TYPE_PACKAGED, "hidden");
  NSString* bundle_id = GetBundleID(shim_path_);

  // It's impractical to confirm that the shim did not launch by timing out, so
  // instead we watch AppLifetimeMonitor::Observer::OnAppActivated.
  AppLifetimeMonitorObserver lifetime_observer(profile());

  // Launch the app. It should create a hidden window, but the shim should not
  // launch.
  {
    ExtensionTestMessageListener launched_listener("Launched");
    LaunchPlatformApp(app);
    EXPECT_TRUE(launched_listener.WaitUntilSatisfied());
  }
  extensions::AppWindow* window_1 = GetFirstAppWindow();
  ASSERT_TRUE(window_1);
  EXPECT_TRUE(window_1->is_hidden());
  EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  EXPECT_EQ(0, lifetime_observer.activated_count());

  // Showing the window causes the shim to launch.
  {
    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer(
        [[WindowedNSNotificationObserver alloc]
            initForWorkspaceNotification:
                NSWorkspaceDidLaunchApplicationNotification
                                bundleId:bundle_id]);
    WindowedAppShimLaunchObserver observer(app->id());
    window_1->Show(extensions::AppWindow::SHOW_INACTIVE);
    EXPECT_TRUE([ns_observer wait]);
    observer.Wait();
    EXPECT_EQ(1, lifetime_observer.activated_count());
    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));
  }

  // Hiding the window causes the shim to quit.
  {
    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer(
        [[WindowedNSNotificationObserver alloc]
            initForWorkspaceNotification:
                NSWorkspaceDidTerminateApplicationNotification
                                bundleId:bundle_id]);
    window_1->Hide();
    EXPECT_TRUE([ns_observer wait]);
    EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  }

  // Launch a second window. It should not launch the shim.
  {
    ExtensionTestMessageListener launched_listener("Launched");
    LaunchPlatformApp(app);
    EXPECT_TRUE(launched_listener.WaitUntilSatisfied());
  }
  const extensions::AppWindowRegistry::AppWindowList& app_windows =
      extensions::AppWindowRegistry::Get(profile())->app_windows();
  EXPECT_EQ(2u, app_windows.size());
  extensions::AppWindow* window_2 = app_windows.front();
  EXPECT_NE(window_1, window_2);
  ASSERT_TRUE(window_2);
  EXPECT_TRUE(window_2->is_hidden());
  EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  EXPECT_EQ(1, lifetime_observer.activated_count());

  // Showing one of the windows should launch the shim.
  {
    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer(
        [[WindowedNSNotificationObserver alloc]
            initForWorkspaceNotification:
                NSWorkspaceDidLaunchApplicationNotification
                                bundleId:bundle_id]);
    WindowedAppShimLaunchObserver observer(app->id());
    window_1->Show(extensions::AppWindow::SHOW_INACTIVE);
    EXPECT_TRUE([ns_observer wait]);
    observer.Wait();
    EXPECT_EQ(2, lifetime_observer.activated_count());
    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));
    EXPECT_TRUE(window_2->is_hidden());
  }

  // Showing the other window does nothing.
  {
    window_2->Show(extensions::AppWindow::SHOW_INACTIVE);
    EXPECT_EQ(2, lifetime_observer.activated_count());
  }

  // Showing an already visible window does nothing.
  {
    window_1->Show(extensions::AppWindow::SHOW_INACTIVE);
    EXPECT_EQ(2, lifetime_observer.activated_count());
  }

  // Hiding one window does nothing.
  {
    AppLifetimeMonitorObserver deactivate_observer(profile());
    window_1->Hide();
    EXPECT_EQ(0, deactivate_observer.deactivated_count());
  }

  // Hiding other window causes the shim to quit.
  {
    AppLifetimeMonitorObserver deactivate_observer(profile());
    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));
    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer(
        [[WindowedNSNotificationObserver alloc]
            initForWorkspaceNotification:
                NSWorkspaceDidTerminateApplicationNotification
                                bundleId:bundle_id]);
    window_2->Hide();
    EXPECT_TRUE([ns_observer wait]);
    EXPECT_EQ(1, deactivate_observer.deactivated_count());
    EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  }
}

#if defined(ARCH_CPU_64_BITS)

// Tests that a 32 bit shim attempting to launch 64 bit Chrome will eventually
// be rebuilt.
IN_PROC_BROWSER_TEST_F(AppShimInteractiveTest, MAYBE_RebuildShim) {
  // Get the 32 bit shim.
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath shim_path_32 =
      test_data_dir.Append("app_shim").Append("app_shim_32_bit.app");
  EXPECT_TRUE(base::PathExists(shim_path_32));

  // Install test app.
  const extensions::Extension* app = InstallPlatformApp("minimal");

  // Use WebAppShortcutCreator to create a 64 bit shim.
  std::unique_ptr<web_app::ShortcutInfo> shortcut_info =
      web_app::ShortcutInfoForExtensionAndProfile(app, profile());
  web_app::WebAppShortcutCreator shortcut_creator(
      web_app::GetOsIntegrationResourcesDirectoryForApp(profile()->GetPath(),
                                                        app->id(), GURL()),
      shortcut_info.get());
  std::vector<base::FilePath> updated_paths;
  shortcut_creator.UpdateShortcuts(false, &updated_paths);
  base::FilePath shim_path = updated_paths.front();
  NSMutableDictionary* plist_64 = [NSMutableDictionary
      dictionaryWithContentsOfFile:base::mac::FilePathToNSString(
                                       shim_path.Append("Contents")
                                           .Append("Info.plist"))];

  // Copy 32 bit shim to where it's expected to be.
  // CopyDirectory doesn't seem to work when copying and renaming in one go.
  ASSERT_TRUE(base::DeletePathRecursively(shim_path));
  ASSERT_TRUE(base::PathExists(shim_path.DirName()));
  ASSERT_TRUE(base::CopyDirectory(shim_path_32, shim_path.DirName(), true));
  ASSERT_TRUE(base::Move(shim_path.DirName().Append(shim_path_32.BaseName()),
                         shim_path));
  ASSERT_TRUE(base::PathExists(
      shim_path.Append("Contents").Append("MacOS").Append("app_mode_loader")));

  // Fix up the plist so that it matches the installed test app.
  NSString* plist_path = base::mac::FilePathToNSString(
      shim_path.Append("Contents").Append("Info.plist"));
  NSMutableDictionary* plist =
      [NSMutableDictionary dictionaryWithContentsOfFile:plist_path];

  NSArray* keys_to_copy = @[
    base::mac::CFToNSCast(kCFBundleIdentifierKey),
    base::mac::CFToNSCast(kCFBundleNameKey), app_mode::kCrAppModeShortcutIDKey,
    app_mode::kCrAppModeUserDataDirKey, app_mode::kBrowserBundleIDKey
  ];
  for (NSString* key in keys_to_copy) {
    [plist setObject:[plist_64 objectForKey:key] forKey:key];
  }
  [plist writeToFile:plist_path atomically:YES];

  base::mac::RemoveQuarantineAttribute(shim_path);

  // Launch the shim, it should start the app and ultimately connect over IPC.
  // This actually happens in multiple launches of the shim:
  // (1) The shim will fail and instead launch Chrome with --app-id so that the
  //     app starts.
  // (2) Chrome launches the shim in response to an app starting, this time the
  //     shim launches Chrome with --app-shim-error, which causes Chrome to
  //     rebuild the shim.
  // (3) After rebuilding, Chrome again launches the shim and expects it to
  //     behave normally.
  ExtensionTestMessageListener launched_listener("Launched");
  base::CommandLine shim_cmdline(base::CommandLine::NO_PROGRAM);

  base::test::TestFuture<base::expected<NSRunningApplication*, NSError*>>
      open_result;
  base::mac::LaunchApplication(shim_path, shim_cmdline, /*url_specs=*/{},
                               /*options=*/{}, open_result.GetCallback());
  ASSERT_TRUE(open_result.Get<0>().has_value());
  NSRunningApplication* shim_app = open_result.Get<0>().value();
  ASSERT_TRUE(shim_app);

  // Wait for the app to start (1). At this point there is no shim host.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  EXPECT_FALSE(HasAppShimHost(profile(), app->id()));

  // Wait for the rebuilt shim to connect (3). This does not race with the app
  // starting (1) because Chrome only launches the shim (2) after the app
  // starts. Then Chrome must handle --app-shim-error on the UI thread before
  // the shim is rebuilt.
  WindowedAppShimLaunchObserver(app->id()).Wait();

  EXPECT_TRUE(GetFirstAppWindow());
  EXPECT_TRUE(HasAppShimHost(profile(), app->id()));
}

#endif  // defined(ARCH_CPU_64_BITS)

}  // namespace apps
