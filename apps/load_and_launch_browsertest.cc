// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the --load-and-launch-app switch.
// The two cases are when chrome is running and another process uses the switch
// and when chrome is started from scratch.

#include <iterator>

#include "apps/switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "sandbox/policy/switches.h"

using extensions::PlatformAppBrowserTest;

namespace apps {

namespace {

constexpr char kTestExtensionId[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";

// Lacros doesn't support launching with chrome already running. See the header
// comment for InProcessBrowserTest::GetCommandLineForRelaunch().
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

const char* const kSwitchesToCopy[] = {
    sandbox::policy::switches::kNoSandbox,
    switches::kUserDataDir,
};

// TODO(jackhou): Enable this test once it works on OSX. It currently does not
// work for the same reason --app-id doesn't. See http://crbug.com/148465
#if BUILDFLAG(IS_MAC)
#define MAYBE_LoadAndLaunchAppChromeRunning \
        DISABLED_LoadAndLaunchAppChromeRunning
#else
#define MAYBE_LoadAndLaunchAppChromeRunning LoadAndLaunchAppChromeRunning
#endif

// Case where Chrome is already running.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_LoadAndLaunchAppChromeRunning) {
  ExtensionTestMessageListener launched_listener("Launched");

  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  base::CommandLine new_cmdline(cmdline.GetProgram());
  new_cmdline.CopySwitchesFrom(cmdline, kSwitchesToCopy);

  base::FilePath app_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("minimal");

  new_cmdline.AppendSwitchNative(apps::kLoadAndLaunchApp,
                                 app_path.value());

  new_cmdline.AppendSwitch(switches::kLaunchAsBrowser);
  base::Process process =
      base::LaunchProcess(new_cmdline, base::LaunchOptionsForTest());
  ASSERT_TRUE(process.IsValid());

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  int exit_code;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  ASSERT_EQ(chrome::RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED, exit_code);
}

// TODO(jackhou): Enable this test once it works on OSX. It currently does not
// work for the same reason --app-id doesn't. See http://crbug.com/148465.
#if BUILDFLAG(IS_MAC)
#define MAYBE_LoadAndLaunchAppWithFile DISABLED_LoadAndLaunchAppWithFile
#else
#define MAYBE_LoadAndLaunchAppWithFile LoadAndLaunchAppWithFile
#endif

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       MAYBE_LoadAndLaunchAppWithFile) {
  ExtensionTestMessageListener launched_listener("Launched");

  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  base::CommandLine new_cmdline(cmdline.GetProgram());
  new_cmdline.CopySwitchesFrom(cmdline, kSwitchesToCopy);

  base::FilePath app_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("load_and_launch_file");

  base::FilePath test_file_path = test_data_dir_
      .AppendASCII("platform_apps")
      .AppendASCII("launch_files")
      .AppendASCII("test.txt");

  new_cmdline.AppendSwitchNative(apps::kLoadAndLaunchApp,
                                 app_path.value());
  new_cmdline.AppendSwitch(switches::kLaunchAsBrowser);
  new_cmdline.AppendArgPath(test_file_path);

  base::Process process =
      base::LaunchProcess(new_cmdline, base::LaunchOptionsForTest());
  ASSERT_TRUE(process.IsValid());

  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  int exit_code;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  ASSERT_EQ(chrome::RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED, exit_code);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// TestFixture that appends --load-and-launch-app with an app before calling
// BrowserMain.
class LoadAndLaunchPlatformAppBrowserTest : public PlatformAppBrowserTest {
 public:
  LoadAndLaunchPlatformAppBrowserTest(
      const LoadAndLaunchPlatformAppBrowserTest&) = delete;
  LoadAndLaunchPlatformAppBrowserTest& operator=(
      const LoadAndLaunchPlatformAppBrowserTest&) = delete;

 protected:
  LoadAndLaunchPlatformAppBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    base::FilePath app_path =
        test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal");
    command_line->AppendSwitchNative(apps::kLoadAndLaunchApp, app_path.value());

    // |launched_listener_| needs to be instantiated before the app process is
    // launched to ensure the test api observer is registered.
    launched_listener_ =
        std::make_unique<ExtensionTestMessageListener>("Launched");
  }

  void TearDownOnMainThread() override { launched_listener_.reset(); }

  void LoadAndLaunchApp() {
    ASSERT_TRUE(launched_listener_->WaitUntilSatisfied());

    // Start an actual browser because we can't shut down with just an app
    // window.
    CreateBrowser(profile());
  }

  std::unique_ptr<ExtensionTestMessageListener> launched_listener_;
};

// TestFixture that appends --load-and-launch-app with an extension before
// calling BrowserMain.
class LoadAndLaunchExtensionBrowserTest : public PlatformAppBrowserTest {
 public:
  LoadAndLaunchExtensionBrowserTest(const LoadAndLaunchExtensionBrowserTest&) =
      delete;
  LoadAndLaunchExtensionBrowserTest& operator=(
      const LoadAndLaunchExtensionBrowserTest&) = delete;

 protected:
  LoadAndLaunchExtensionBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    base::FilePath app_path = test_data_dir_.AppendASCII("good")
                                  .AppendASCII("Extensions")
                                  .AppendASCII(kTestExtensionId)
                                  .AppendASCII("1.0.0.0");
    command_line->AppendSwitchNative(apps::kLoadAndLaunchApp, app_path.value());
  }

  void SetUpInProcessBrowserTestFixture() override {
    PlatformAppBrowserTest::SetUpInProcessBrowserTestFixture();

    // Skip showing the error message box to avoid freezing the main thread.
    chrome::internal::g_should_skip_message_box_for_test = true;
  }
};

// Case where Chrome is not running.
IN_PROC_BROWSER_TEST_F(LoadAndLaunchPlatformAppBrowserTest,
                       LoadAndLaunchAppChromeNotRunning) {
  LoadAndLaunchApp();
}

IN_PROC_BROWSER_TEST_F(LoadAndLaunchExtensionBrowserTest,
                       LoadAndLaunchExtension) {
  const std::vector<std::u16string>* errors =
      extensions::LoadErrorReporter::GetInstance()->GetErrors();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The error is skipped on official builds.
  EXPECT_TRUE(errors->empty());
#else
  // Expect |extension_instead_of_app_error|.
  EXPECT_EQ(1u, errors->size());
  EXPECT_TRUE(base::Contains(
      *errors, u"App loading flags cannot be used to load extensions"));
#endif

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  EXPECT_EQ(nullptr,
            registry->GetExtensionById(
                kTestExtensionId, extensions::ExtensionRegistry::EVERYTHING));
}

}  // namespace
}  // namespace apps
