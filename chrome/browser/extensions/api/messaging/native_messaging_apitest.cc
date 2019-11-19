// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
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
#include "extensions/browser/process_manager.h"
#include "extensions/test/result_catcher.h"

namespace extensions {
namespace {

class NativeMessagingApiTest : public ExtensionApiTest {
 protected:
  extensions::ScopedTestNativeMessagingHost test_host_;
};

IN_PROC_BROWSER_TEST_F(NativeMessagingApiTest, NativeMessagingBasic) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(false));
  ASSERT_TRUE(RunExtensionTest("native_messaging")) << message_;
}

IN_PROC_BROWSER_TEST_F(NativeMessagingApiTest, UserLevelNativeMessaging) {
  ASSERT_NO_FATAL_FAILURE(test_host_.RegisterTestHost(true));
  ASSERT_TRUE(RunExtensionTest("native_messaging")) << message_;
}

#if !defined(OS_CHROMEOS)

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

    observer_.Add(process_manager);
    run_loop.Run();
  }

 private:
  void OnBackgroundHostClose(const std::string& extension_id) override {
    if (extension_id != extension_id_) {
      return;
    }
    observer_.RemoveAll();
    extension_id_.clear();
    std::move(quit_).Run();
  }

  std::string extension_id_;
  ScopedObserver<ProcessManager, ProcessManagerObserver> observer_{this};
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
  return command_line;
}

class NativeMessagingLaunchApiTest : public NativeMessagingApiTest {
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

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      CreateNativeMessagingConnectCommandLine("test-connect-id"), {},
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
    ScopedObserver<KeepAliveRegistry, KeepAliveStateObserver> observer(this);
    observer.Add(KeepAliveRegistry::GetInstance());

    // On Mac, the browser remains alive when no windows are open, so observing
    // the KeepAliveRegistry cannot detect when the native messaging keep-alive
    // has been released; poll for changes instead.
#if defined(OS_MACOSX)
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

#if defined(OS_MACOSX)
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

#endif  // !defined(OS_CHROMEOS)

}  // namespace
}  // namespace extensions
