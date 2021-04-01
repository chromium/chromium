// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
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
#include "extensions/test/result_catcher.h"

namespace extensions {
namespace {

using ContextType = ExtensionApiTest::ContextType;

class NativeMessagingApiTestBase : public ExtensionApiTest {
 protected:
  extensions::ScopedTestNativeMessagingHost test_host_;
};

class NativeMessagingApiTest : public NativeMessagingApiTestBase,
                               public testing::WithParamInterface<ContextType> {
 protected:
  bool RunTest(const char* extension_name) {
    if (GetParam() == ContextType::kPersistentBackground)
      return RunExtensionTest({.name = extension_name});
    std::string lazy_exension_name = base::StrCat({extension_name, "/lazy"});
    return RunExtensionTest(
        {.name = lazy_exension_name.c_str()},
        {.load_as_service_worker = GetParam() == ContextType::kServiceWorker});
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

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)

class TestProcessManagerObserver : public ProcessManagerObserver {
 public:
  TestProcessManagerObserver() = default;
  ~TestProcessManagerObserver() override = default;

  void WaitForProcessShutdown(ProcessManager* process_manager,
                              const std::string& extension_id) {
    DCHECK(!quit_);
    extension_id_ = extension_id;
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();

    observation_.Observe(process_manager);
    run_loop.Run();
  }

 private:
  void OnBackgroundHostClose(const std::string& extension_id) override {
    if (extension_id != extension_id_) {
      return;
    }
    observation_.Reset();
    extension_id_.clear();
    std::move(quit_).Run();
  }

  std::string extension_id_;
  base::ScopedObservation<ProcessManager, ProcessManagerObserver> observation_{
      this};
  base::OnceClosure quit_;

  DISALLOW_COPY_AND_ASSIGN(TestProcessManagerObserver);
};

base::CommandLine CreateNativeMessagingConnectCommandLine(
    const std::string& connect_id,
    const std::string& extension_id =
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

// Disabled on Windows due to timeouts; see https://crbug.com/984897.
#if defined(OS_WIN)
#define MAYBE_Success DISABLED_Success
#else
#define MAYBE_Success Success
#endif
IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchApiTest, MAYBE_Success) {
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));

  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("native_messaging_launch"));
  TestProcessManagerObserver observer;
  observer.WaitForProcessShutdown(ProcessManager::Get(profile()),
                                  extension->id());

  ResultCatcher catcher;

  EXPECT_FALSE(
      g_browser_process->background_mode_manager()->IsBackgroundModeActive());

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      CreateNativeMessagingConnectCommandLine("test-connect-id"), {},
      profile()->GetPath());

  EXPECT_TRUE(
      g_browser_process->background_mode_manager()->IsBackgroundModeActive());

  if (!catcher.GetNextResult()) {
    FAIL() << catcher.message();
  }
  size_t tabs = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    tabs += browser->tab_strip_model()->count();
  }
  EXPECT_EQ(1u, tabs);
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
  TestProcessManagerObserver observer;
  observer.WaitForProcessShutdown(ProcessManager::Get(profile()),
                                  extension->id());

  ResultCatcher catcher;

  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectExtension,
                                 extension->id());
  command_line.AppendSwitchASCII(switches::kNativeMessagingConnectHost,
                                 ScopedTestNativeMessagingHost::kHostName);
  command_line.AppendSwitch(switches::kNoStartupWindow);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(command_line, {},
                                                          profile()->GetPath());

  if (!catcher.GetNextResult()) {
    FAIL() << catcher.message();
  }
  size_t tabs = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    tabs += browser->tab_strip_model()->count();
  }
  EXPECT_EQ(1u, tabs);
}

class TestKeepAliveStateObserver : public KeepAliveStateObserver {
 public:
  TestKeepAliveStateObserver() = default;

  void WaitForNoKeepAlive() {
    ASSERT_TRUE(KeepAliveRegistry::GetInstance()->IsKeepingAlive());
    base::ScopedObservation<KeepAliveRegistry, KeepAliveStateObserver> observer(
        this);
    observer.Observe(KeepAliveRegistry::GetInstance());

    // On Mac, the browser remains alive when no windows are open, so observing
    // the KeepAliveRegistry cannot detect when the native messaging keep-alive
    // has been released; poll for changes instead.
#if defined(OS_MAC)
    polling_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(100),
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

#if defined(OS_MAC)
  void PollKeepAlive() {
    OnKeepAliveStateChanged(
        KeepAliveRegistry::GetInstance()->IsOriginRegistered(
            KeepAliveOrigin::NATIVE_MESSAGING_HOST_ERROR_REPORT));
  }

  base::RepeatingTimer polling_timer_;
#endif

  base::OnceClosure quit_;

  DISALLOW_COPY_AND_ASSIGN(TestKeepAliveStateObserver);
};

IN_PROC_BROWSER_TEST_F(NativeMessagingLaunchApiTest, Error) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));
  ScopedNativeMessagingErrorTimeoutOverrideForTest error_timeout_override(
      base::TimeDelta::FromSeconds(2));
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      CreateNativeMessagingConnectCommandLine("test-connect-id"), {},
      profile()->GetPath());
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
      profile()->GetPath());
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
      profile()->GetPath());
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
      profile()->GetPath());
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
#if defined(OS_WIN)
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
  size_t tabs = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    tabs += browser->tab_strip_model()->count();
  }
  EXPECT_EQ(0u, tabs);

  ASSERT_NO_FATAL_FAILURE(TestKeepAliveStateObserver().WaitForNoKeepAlive());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace
}  // namespace extensions
