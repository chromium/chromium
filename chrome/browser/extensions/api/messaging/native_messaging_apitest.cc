// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_state_observer.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/result_catcher.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#endif

namespace extensions {
namespace {

using ContextType = ExtensionApiTest::ContextType;

class NativeMessagingApiTestBase : public ExtensionApiTest {
 public:
  explicit NativeMessagingApiTestBase(
      ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~NativeMessagingApiTestBase() override = default;
  NativeMessagingApiTestBase(const NativeMessagingApiTestBase&) = delete;
  NativeMessagingApiTestBase& operator=(const NativeMessagingApiTestBase&) =
      delete;

 protected:
  size_t GetTotalTabCount() const {
    size_t tabs = 0;
    for (WindowController* window : *WindowControllerList::GetInstance()) {
      tabs += window->GetTabCount();
    }
    return tabs;
  }

  extensions::ScopedTestNativeMessagingHost test_host_;
};

// Tests basic functionality of chrome.runtime.sendNativeMessage in an MV3
// extension.
IN_PROC_BROWSER_TEST_F(NativeMessagingApiTestBase, SendNativeMessage) {
  constexpr bool kUserLevel = false;
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(kUserLevel));
  ASSERT_TRUE(RunExtensionTest("native_messaging_send_native_message"));
}

IN_PROC_BROWSER_TEST_F(NativeMessagingApiTestBase, UserLevelSendNativeMessage) {
  constexpr bool kUserLevel = true;
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(kUserLevel));
  ASSERT_TRUE(RunExtensionTest("native_messaging_send_native_message"));
}

#if BUILDFLAG(IS_WIN)
// On Windows, a new codepath is used to directly launch .EXE-based Native
// Hosts. This codepath allows launching of Native Hosts even when cmd.exe is
// disabled or misconfigured.
class NativeMessagingLaunchExeTest : public NativeMessagingApiTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  NativeMessagingLaunchExeTest() {
    feature_list_.InitWithFeatureState(
        extensions_features::kLaunchWindowsNativeHostsDirectly,
        IsDirectLaunchEnabled());
  }

  bool IsDirectLaunchEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(NativeMessagingLaunchExe,
                         NativeMessagingLaunchExeTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(NativeMessagingLaunchExeTest,
                       UserLevelSendNativeMessageWinExe) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestExeHost(
      "native_messaging_test_echo_host.exe", /*user_level=*/true));

  ASSERT_TRUE(RunExtensionTest("native_messaging_send_native_message_exe"));
}

// The Host's filename deliberately contains the character '&' which causes the
// Host to fail to launch if cmd.exe is used as an intermediary between the
// extension and the host executable, unless extra quotes are used.
// crbug.com/335558
IN_PROC_BROWSER_TEST_P(NativeMessagingLaunchExeTest,
                       SendNativeMessageWinExeAmpersand) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestExeHost(
      "native_messaging_test_echo_&_host.exe", /*user_level=*/false));

  ASSERT_TRUE(RunExtensionTest("native_messaging_send_native_message_exe"));
}

// Make sure that a filename with a space is supported.
IN_PROC_BROWSER_TEST_P(NativeMessagingLaunchExeTest,
                       SendNativeMessageWinExeSpace) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestExeHost(
      "native_messaging_test_echo_ _host.exe", /*user_level=*/false));

  ASSERT_TRUE(RunExtensionTest("native_messaging_send_native_message_exe"));
}
#endif

class NativeMessagingApiTest : public NativeMessagingApiTestBase,
                               public testing::WithParamInterface<ContextType> {
 public:
  NativeMessagingApiTest() : NativeMessagingApiTestBase(GetParam()) {}
  ~NativeMessagingApiTest() override = default;
  NativeMessagingApiTest(const NativeMessagingApiTest&) = delete;
  NativeMessagingApiTest& operator=(const NativeMessagingApiTest&) = delete;

 protected:
  bool RunTest(const char* extension_name) {
    if (GetParam() == ContextType::kPersistentBackground)
      return RunExtensionTest(extension_name);
    std::string lazy_extension_name = base::StrCat({extension_name, "/lazy"});
    return RunExtensionTest(lazy_extension_name.c_str());
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         NativeMessagingApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(EventPage,
                         NativeMessagingApiTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         NativeMessagingApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Tests chrome.runtime.sendNativeMessage to a native messaging host.
IN_PROC_BROWSER_TEST_P(NativeMessagingApiTest, NativeMessagingBasic) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));
  ASSERT_TRUE(RunTest("native_messaging")) << message_;
}

IN_PROC_BROWSER_TEST_P(NativeMessagingApiTest, UserLevelNativeMessaging) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(true));
  ASSERT_TRUE(RunTest("native_messaging")) << message_;
}

// Tests chrome.runtime.connectNative to a native messaging host.
IN_PROC_BROWSER_TEST_P(NativeMessagingApiTest, ConnectNative) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));
  ASSERT_TRUE(RunTest("native_messaging_connect")) << message_;
}

IN_PROC_BROWSER_TEST_P(NativeMessagingApiTest,
                       UserLevelNativeMessagingConnectNative) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(true));
  ASSERT_TRUE(RunTest("native_messaging_connect")) << message_;
}

#if !BUILDFLAG(IS_CHROMEOS)

base::CommandLine CreateNativeMessagingConnectCommandLine(
    const std::string& connect_id,
    const ExtensionId& extension_id =
        ScopedTestNativeMessagingHost::kExtensionId) {
  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectExtension,
                                 extension_id);
  command_line.AppendSwitchASCII(
      switches::kNativeMessagingConnectHost,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName);
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectId,
                                 connect_id);
  command_line.AppendSwitch(switches::kNoStartupWindow);
  return command_line;
}

class NativeMessagingLaunchApiTest : public NativeMessagingApiTestBase {
 public:
  NativeMessagingLaunchApiTest() {
    feature_list_.InitAndEnableFeature(features::kOnConnectNative);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchApiTest, Success) {
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));

  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("native_messaging_launch"));
  ExtensionBackgroundPageWaiter(profile(), *extension)
      .WaitForBackgroundClosed();

  ResultCatcher catcher;

  EXPECT_FALSE(
      g_browser_process->background_mode_manager()->IsBackgroundModeActive());

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      CreateNativeMessagingConnectCommandLine("test-connect-id"), {},
      {profile()->GetPath(), StartupProfileModeReason::kAppRequested});

  EXPECT_TRUE(
      g_browser_process->background_mode_manager()->IsBackgroundModeActive());

  if (!catcher.GetNextResult()) {
    FAIL() << catcher.message();
  }
  EXPECT_EQ(1u, GetTotalTabCount());
}

// Test that a natively-initiated connection from a host not supporting
// natively-initiated connections is not allowed. The test extension expects the
// channel to be immediately closed with an error.
IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchApiTest, UnsupportedByNativeHost) {
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  extensions::ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(false));

  auto* extension = LoadExtension(
      test_data_dir_.AppendASCII("native_messaging_launch_unsupported"));
  ExtensionBackgroundPageWaiter(profile(), *extension)
      .WaitForBackgroundClosed();

  ResultCatcher catcher;

  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectExtension,
                                 extension->id());
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectHost,
                                 ScopedTestNativeMessagingHost::kHostName);
  command_line.AppendSwitch(switches::kNoStartupWindow);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, {},
      {profile()->GetPath(), StartupProfileModeReason::kAppRequested});

  if (!catcher.GetNextResult()) {
    FAIL() << catcher.message();
  }
  EXPECT_EQ(1u, GetTotalTabCount());
}

class TestKeepAliveStateObserver : public KeepAliveStateObserver {
 public:
  TestKeepAliveStateObserver() = default;

  TestKeepAliveStateObserver(const TestKeepAliveStateObserver&) = delete;
  TestKeepAliveStateObserver& operator=(const TestKeepAliveStateObserver&) =
      delete;

  void WaitForNoKeepAlive() {
    ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
    base::ScopedObservation<KeepAliveRegistry, KeepAliveStateObserver> observer(
        this);
    observer.Observe(KeepAliveRegistry::GetInstance());

    // On Mac, the browser remains alive when no windows are open, so observing
    // the KeepAliveRegistry cannot detect when the native messaging keep-alive
    // has been released; poll for changes instead.
#if BUILDFLAG(IS_MAC)
    polling_timer_.Start(
        FROM_HERE, base::Milliseconds(100),
        base::BindRepeating(&TestKeepAliveStateObserver::PollKeepAlive,
                            base::Unretained(this)));
#endif

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnKeepAliveStateChanged(bool is_keeping_alive) override {
    if (!is_keeping_alive && quit_) {
      std::move(quit_).Run();
    }
  }

  void OnKeepAliveRestartStateChanged(bool can_restart) override {}

#if BUILDFLAG(IS_MAC)
  void PollKeepAlive() {
    OnKeepAliveStateChanged(
        KeepAliveRegistry::GetInstance()->IsOriginRegistered(
            KeepAliveOrigin::NATIVE_MESSAGING_HOST_ERROR_REPORT));
  }

  base::RepeatingTimer polling_timer_;
#endif

  base::OnceClosure quit_;
};

IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchApiTest, Error) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));
  ScopedNativeMessagingErrorTimeoutOverrideForTest error_timeout_override(
      base::Seconds(2));
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      CreateNativeMessagingConnectCommandLine("test-connect-id"), {},
      {profile()->GetPath(), StartupProfileModeReason::kAppRequested});
  ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NATIVE_MESSAGING_HOST_ERROR_REPORT));

  // Close the browser so the native messaging host error reporting is the only
  // keep-alive.
  browser()->window()->Close();

  ASSERT_NO_FATAL_FAILURE(TestKeepAliveStateObserver().WaitForNoKeepAlive());

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string connect_id;
  ASSERT_TRUE(base::ReadFileToString(
      test_host_.temp_dir().AppendASCII("connect_id.txt"), &connect_id));
  EXPECT_EQ("--connect-id=test-connect-id", connect_id);
}

IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchApiTest, InvalidConnectId) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      CreateNativeMessagingConnectCommandLine("\"connect id!\""), {},
      {profile()->GetPath(), StartupProfileModeReason::kAppRequested});
  ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NATIVE_MESSAGING_HOST_ERROR_REPORT));

  // Close the browser so the native messaging host error reporting is the only
  // keep-alive.
  browser()->window()->Close();

  ASSERT_NO_FATAL_FAILURE(TestKeepAliveStateObserver().WaitForNoKeepAlive());

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(
      test_host_.temp_dir().AppendASCII("invalid_connect_id.txt"), &content));
  EXPECT_EQ("--invalid-connect-id", content);
}

IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchApiTest, TooLongConnectId) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      CreateNativeMessagingConnectCommandLine(std::string(21, 'a')), {},
      {profile()->GetPath(), StartupProfileModeReason::kAppRequested});
  ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NATIVE_MESSAGING_HOST_ERROR_REPORT));

  // Close the browser so the native messaging host error reporting is the only
  // keep-alive.
  browser()->window()->Close();

  ASSERT_NO_FATAL_FAILURE(TestKeepAliveStateObserver().WaitForNoKeepAlive());

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(
      test_host_.temp_dir().AppendASCII("invalid_connect_id.txt"), &content));
  EXPECT_EQ("--invalid-connect-id", content);
}

IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchApiTest, InvalidExtensionId) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      CreateNativeMessagingConnectCommandLine("test-connect-id", "abcd"), {},
      {profile()->GetPath(), StartupProfileModeReason::kAppRequested});
  ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::NATIVE_MESSAGING_HOST_ERROR_REPORT));

  // Close the browser so the native messaging host error reporting is the only
  // keep-alive.
  browser()->window()->Close();

  ASSERT_NO_FATAL_FAILURE(TestKeepAliveStateObserver().WaitForNoKeepAlive());

  base::ScopedAllowBlockingForTesting allow_blocking;

  // The native messaging host should not have launched and so neither error
  // reporting file should have been created.
  EXPECT_FALSE(
      base::PathExists(test_host_.temp_dir().AppendASCII("connect_id.txt")));
  EXPECT_FALSE(base::PathExists(
      test_host_.temp_dir().AppendASCII("invalid_connect_id.txt")));
}

constexpr char kExtensionId[] = "knldjmfmopnpolahpmmgbagdohdnhkik";

class NativeMessagingLaunchBackgroundModeApiTest
    : public NativeMessagingLaunchApiTest {
 public:
  NativeMessagingLaunchBackgroundModeApiTest() {
    ProcessManager::SetEventPageIdleTimeForTesting(1);
    ProcessManager::SetEventPageSuspendingTimeForTesting(1);
    test_host_.RegisterTestHost(true);
  }

  NativeMessagingLaunchBackgroundModeApiTest(
      const NativeMessagingLaunchBackgroundModeApiTest&) = delete;
  NativeMessagingLaunchBackgroundModeApiTest& operator=(
      const NativeMessagingLaunchBackgroundModeApiTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    NativeMessagingLaunchApiTest::SetUpCommandLine(command_line);

    if (base::StartsWith(
            ::testing::UnitTest::GetInstance()->current_test_info()->name(),
            "PRE")) {
      return;
    }
    set_exit_when_last_browser_closes(false);
    command_line->AppendSwitchASCII(switches::kNativeMessagingConnectExtension,
                                    kExtensionId);
    command_line->AppendSwitchASCII(
        switches::kNativeMessagingConnectHost,
        ScopedTestNativeMessagingHost::
            kSupportsNativeInitiatedConnectionsHostName);
    command_line->AppendSwitchASCII(switches::kNativeMessagingConnectId,
                                    "test-connect-id");
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  void SetUpOnMainThread() override {
    NativeMessagingLaunchApiTest::SetUpOnMainThread();

    catcher_ = std::make_unique<ResultCatcher>();
  }

  std::unique_ptr<ResultCatcher> catcher_;
};

IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchBackgroundModeApiTest,
                       PRE_Success) {
  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("native_messaging_launch"));
  EXPECT_EQ(kExtensionId, extension->id());
}

// Flaky on a Windows bot. See crbug.com/1030332.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Success DISABLED_Success
#else
#define MAYBE_Success Success
#endif
IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchBackgroundModeApiTest,
                       MAYBE_Success) {
  EXPECT_TRUE(
      g_browser_process->background_mode_manager()->IsBackgroundModeActive());

  if (!catcher_->GetNextResult()) {
    FAIL() << catcher_->message();
  }
  EXPECT_EQ(0u, GetTotalTabCount());

  ASSERT_NO_FATAL_FAILURE(TestKeepAliveStateObserver().WaitForNoKeepAlive());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
class NativeHostExecutablesLaunchDirectlyPolicyTest
    : public extensions::NativeMessagingApiTestBase,
      public testing::WithParamInterface<bool> {
 public:
  NativeHostExecutablesLaunchDirectlyPolicyTest() {
    feature_list_.InitWithFeatureState(
        extensions_features::kLaunchWindowsNativeHostsDirectly,
        IsDirectLaunchEnabled());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDownOnMainThread() override {
    profile_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  bool IsDirectLaunchEnabled() const { return GetParam(); }

 protected:
  extensions::ScopedTestNativeMessagingHost test_host_;
  std::unique_ptr<TestingProfile> profile_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(NativeHostExecutablesLaunchDirectlyPolicyTest,
                       PolicyDisabledTest) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kNativeHostsExecutablesLaunchDirectly, true);

  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestExeHost(
      "native_messaging_test_echo_&_host.exe", /*user_level=*/false));

  ASSERT_TRUE(RunExtensionTest("native_messaging_send_native_message_exe"));
}

IN_PROC_BROWSER_TEST_P(NativeHostExecutablesLaunchDirectlyPolicyTest,
                       PolicyEnabledTest) {
  PrefService* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kNativeHostsExecutablesLaunchDirectly, false);

  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestExeHost(
      "native_messaging_test_echo_&_host.exe", /*user_level=*/false));

  ASSERT_TRUE(RunExtensionTest("native_messaging_send_native_message_exe"));
}

INSTANTIATE_TEST_SUITE_P(NativeHostExecutablesLaunchDirectlyPolicyTestP,
                         NativeHostExecutablesLaunchDirectlyPolicyTest,
                         testing::Bool());
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace extensions
