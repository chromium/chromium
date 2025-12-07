// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#include <inttypes.h>
#include <stdint.h>

#include "base/apple/bundle_locations.h"
#include "base/at_exit.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/launchservices_utils_mac.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/remote_cocoa/app_shim/application_bridge.h"
#include "components/remote_cocoa/app_shim/browser_native_widget_window_mac.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/multiprocess_func_list.h"
#include "url/gurl.h"

extern "C" {
int APP_SHIM_ENTRY_POINT_NAME(const app_mode::ChromeAppModeInfo* info);
}

#if !defined(OFFICIAL_BUILD)
// This test relies on disabling some code signing checks to work. That is safe
// since we only allow that in unsigned builds, but to be extra secure, the
// relevant code is also only included in unofficial builds. Since most bots
// test with such builds, this still gives us most of the desired test coverage.

namespace web_app {
namespace {
constexpr std::string_view kAppShimPathSwitch = "app-shim-path";
constexpr std::string_view kWebAppIdSwitch = "web-app-id";
constexpr std::string_view kShimLogFileName = "shim.log";
}  // namespace

using AppId = webapps::AppId;

// AppShimController does not live in the browser process, rather it lives in
// an app shim process. Because of this, a regular browser test can not really
// be used to test its implementation. Instead what this browser test does is:
//   - In SetUpOnMainThread (which runs in the browser process), install a test
//     PWA, as setup for the actual test.
//   - Use the "multi process test" support to launch a secondary test process
//     which will act as the app shim process. The actual test body lives in
//     that process.
//   - And currently, the only test specifically tests what happens when Chrome
//     isn't already running. To do this, the test is triggered from SetUp()
//     after InProcessBrowserTest::SetUp() returns. At this point the "test"
//     browser has terminated, allowing us to test this behavior.
class AppShimControllerBrowserTest : public InProcessBrowserTest {
 public:
  AppShimControllerBrowserTest() = default;
  ~AppShimControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Install test PWA, and record the app id and path of generated app shim.
    ASSERT_TRUE(embedded_test_server()->Start());
    const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    app_id_ = web_app::test::InstallWebApp(browser()->profile(),
                                           std::move(web_app_info));
    auto os_integration = OsIntegrationTestOverrideImpl::Get();
    app_shim_path_ = os_integration->GetShortcutPath(
        browser()->profile(), os_integration->chrome_apps_folder(), app_id_,
        "");
    ASSERT_TRUE(!app_shim_path_.empty());
  }

  void SetUp() override {
    InProcessBrowserTest::SetUp();

    // Now chrome shouldn't be running anymore, so we can do the "real" test.
    base::TimeTicks start_time = base::TimeTicks::Now();

    base::FilePath user_data_dir =
        base::PathService::CheckedGet(chrome::DIR_USER_DATA);

    base::CommandLine command_line(
        base::GetMultiProcessTestChildBaseCommandLine());
    // We need to make sure the child process doesn't initialize NSApp, since we
    // want the app shim code to be able to do that.
    command_line.AppendSwitch(switches::kDoNotCreateNSAppForTests);
    // Pass along various other bits of information the shim process needs.
    command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);
    command_line.AppendSwitchPath(kAppShimPathSwitch, app_shim_path_);
    command_line.AppendSwitchASCII(kWebAppIdSwitch, app_id_);

    // Spawn app shim process, and wait for it to exit.
    base::Process test_child_process = base::SpawnMultiProcessTestChild(
        "AppShimControllerBrowserTestAppShimMain", command_line,
        base::LaunchOptions());
    int rv = -1;
    EXPECT_TRUE(base::WaitForMultiprocessTestChildExit(
        test_child_process, TestTimeouts::action_max_timeout(), &rv));
    EXPECT_EQ(0, rv);

    // To validate that the app shim process did what we expected it to do, it
    // writes a log of its actions to disk. Read that file and verify we had
    // the epxected behavior.
    base::FilePath log_file = user_data_dir.AppendASCII(kShimLogFileName);
    std::string log_string;
    base::ReadFileToString(log_file, &log_string);
    std::vector<std::string> log = base::SplitString(
        log_string, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    EXPECT_THAT(log,
                testing::ElementsAre(
                    "Shim Started", "Window Created: BrowserNativeWidgetWindow",
                    "Window Created: NativeWidgetMacOverlayNSWindow"));

    // If the test failed, it can be hard to debug why without getting output
    // from the Chromium process that was launched by the test. So gather that
    // output from the system log, and include it here.
    if (testing::Test::HasFailure()) {
      base::TimeDelta log_time = base::TimeTicks::Now() - start_time;
      std::vector<std::string> log_argv = {
          "log",
          "show",
          "--process",
          "Chromium",
          "--last",
          base::StringPrintf("%" PRId64 "s", log_time.InSeconds() + 1)};
      std::string log_output;
      base::GetAppOutputAndError(log_argv, &log_output);
      LOG(INFO)
          << "System logs during this test run (could include other tests):\n"
          << log_output;
    }
  }

 protected:
  OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  AppId app_id_;
  base::FilePath app_shim_path_;
};

IN_PROC_BROWSER_TEST_F(AppShimControllerBrowserTest, LaunchChrome) {
  // Test body is in SetUp and below in app shim main.
}

class AppShimControllerDelegate : public AppShimController::TestDelegate {
 public:
  void PopulateChromeCommandLine(base::CommandLine& command_line) override {
    test_launcher_utils::PrepareBrowserCommandLineForTests(&command_line);
    command_line.AppendSwitch(switches::kAllowAppShimSignatureMismatchForTests);
  }
};

MULTIPROCESS_TEST_MAIN(AppShimControllerBrowserTestAppShimMain) {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  base::FilePath user_data_dir =
      command_line.GetSwitchValuePath(switches::kUserDataDir);
  // Shim code expects user data dir to contain 3 extra path components that are
  // subsequentially stripped off again.
  std::string user_data_dir_string = user_data_dir.AppendASCII("profile")
                                         .AppendASCII("Web Applications")
                                         .AppendASCII("appid")
                                         .value();
  std::string app_id = command_line.GetSwitchValueASCII(kWebAppIdSwitch);
  std::string app_shim_path =
      command_line.GetSwitchValueASCII(kAppShimPathSwitch);
  base::apple::SetOverrideMainBundlePath(base::FilePath(app_shim_path));
  std::string framework_path = base::apple::FrameworkBundlePath().value();
  std::string chrome_path = ::test::GuessAppBundlePath().value();

  AppShimControllerDelegate controller_delegate;
  AppShimController::SetDelegateForTesting(&controller_delegate);

  // Need to reset a bunch of things before we can run the app shim
  // initialization code.
  base::FeatureList::ClearInstanceForTesting();
  base::CommandLine::Reset();
  base::AtExitManager::AllowShadowingForTesting();

  // Log some data to a log file, so the main test process can validate that the
  // test passed.
  base::File log_file(user_data_dir.AppendASCII(kShimLogFileName),
                      base::File::FLAG_WRITE | base::File::FLAG_CREATE);
  auto log = [&log_file](const std::string& s) {
    log_file.WriteAtCurrentPosAndCheck(base::as_byte_span(s + '\n'));
    log_file.Flush();
  };

  log("Shim Started");

  // Close a browser window when it gets created. This should cause chrome and
  // the shim to terminate, marking the end of the test.
  remote_cocoa::ApplicationBridge::Get()->SetNSWindowCreatedCallbackForTesting(
      base::BindLambdaForTesting([&](NativeWidgetMacNSWindow* window) {
        log(base::StringPrintf("Window Created: %s",
                               base::SysNSStringToUTF8([window className])));
        if ([window isKindOfClass:[BrowserNativeWidgetWindow class]]) {
          [window performClose:nil];
        }
      }));

  app_mode::ChromeAppModeInfo info;
  char* argv[] = {};
  info.argc = 0;
  info.argv = argv;
  info.chrome_framework_path = framework_path.c_str();
  info.chrome_outer_bundle_path = chrome_path.c_str();
  info.app_mode_bundle_path = app_shim_path.c_str();
  info.app_mode_id = app_id.c_str();
  info.app_mode_name = "Basic test app";
  info.app_mode_url = "";
  info.profile_dir = "";
  info.user_data_dir = user_data_dir_string.c_str();
  APP_SHIM_ENTRY_POINT_NAME(&info);

  return 0;
}

}  // namespace web_app

#endif  // !defined(OFFICIAL_BUILD)
