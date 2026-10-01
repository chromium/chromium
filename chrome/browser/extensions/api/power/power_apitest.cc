// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/device_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/power/power_api.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/idle/idle_manager.h"
#include "extensions/browser/api/idle/idle_manager_factory.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/base/idle/scoped_set_idle_state.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

class PowerApiTest : public ExtensionApiTest {
 protected:
  int32_t GetActiveWakeLocks(device::mojom::WakeLockType type) {
    PowerAPI::Get(profile())->FlushWakeLockForTesting();
    mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
    content::GetDeviceService().BindWakeLockProvider(
        wake_lock_provider.BindNewPipeAndPassReceiver());
    base::test::TestFuture<int32_t> future;
    wake_lock_provider->GetActiveWakeLocksForTests(type, future.GetCallback());
    return future.Get();
  }
};

IN_PROC_BROWSER_TEST_F(PowerApiTest, Basics) {
  ASSERT_TRUE(RunExtensionTest("power/basics")) << message_;

  // The test should leave no wake locks (no "level" for any extension).
  EXPECT_TRUE(PowerAPI::Get(profile())->extension_levels().empty());
  EXPECT_EQ(0, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(
      0, GetActiveWakeLocks(device::mojom::WakeLockType::kPreventDisplaySleep));
}

IN_PROC_BROWSER_TEST_F(PowerApiTest, RequestAndChangeWakeLockTypes) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Power API WakeLock Test",
    "version": "1.0",
    "manifest_version": 3,
    "permissions": ["power"],
    "background": {"service_worker": "background.js"}
  })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto run_script = [&](const char* script) {
    BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  run_script(
      "chrome.power.requestKeepAwake('display'); "
      "chrome.test.sendScriptResult('done');");
  EXPECT_EQ(0, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(
      1, GetActiveWakeLocks(device::mojom::WakeLockType::kPreventDisplaySleep));

  run_script(
      "chrome.power.requestKeepAwake('system'); "
      "chrome.test.sendScriptResult('done');");
  EXPECT_EQ(1, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(
      0, GetActiveWakeLocks(device::mojom::WakeLockType::kPreventDisplaySleep));

  run_script(
      "chrome.power.requestKeepAwake('display'); "
      "chrome.test.sendScriptResult('done');");
  EXPECT_EQ(0, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(
      1, GetActiveWakeLocks(device::mojom::WakeLockType::kPreventDisplaySleep));

  run_script(
      "chrome.power.releaseKeepAwake(); "
      "chrome.test.sendScriptResult('done');");
  EXPECT_EQ(0, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(
      0, GetActiveWakeLocks(device::mojom::WakeLockType::kPreventDisplaySleep));

  // Request 'system' (differing from the initial 'display' creation type) and
  // leave it active so extension unload / profile teardown verifies that
  // WakeLockProvider cleans up a type-changed WakeLock without crashing.
  run_script(
      "chrome.power.requestKeepAwake('system'); "
      "chrome.test.sendScriptResult('done');");
  EXPECT_EQ(1, GetActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventAppSuspension));
}

#if BUILDFLAG(IS_CHROMEOS)

constexpr char kExtensionRelativePath[] = "power/report_activity";
constexpr char kExtensionId[] = "dibbenaepdnglcjpgjmnefmjccpinang";

using ContextType = extensions::browser_test_util::ContextType;

class PowerReportActivityApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  PowerReportActivityApiTest() : ExtensionApiTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         PowerReportActivityApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         PowerReportActivityApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Verifies that chrome.power.reportActivity() correctly reports a user activity
// by observing idle states.
IN_PROC_BROWSER_TEST_P(PowerReportActivityApiTest,
                       ReportActivityChangesIdleStateFromIdleToActive) {
  ResultCatcher catcher;

  ExtensionTestMessageListener ready_listener("ready");
  ready_listener.set_extension_id(kExtensionId);

  LoadExtension(test_data_dir_.AppendASCII(kExtensionRelativePath));

  // Wait for idle state listener for be set up.
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  {
    // Wait for the state to change to idle.
    ExtensionTestMessageListener idle_listener("idle");
    ui::ScopedSetIdleState idle(ui::IDLE_STATE_IDLE);
    ASSERT_TRUE(idle_listener.WaitUntilSatisfied());

    // Allow the idle state to go out of scope to reset it.
    // Otherwise the QueryState() call is always going to return
    // the test state, even though it actually changed.
  }

  auto* idle_manager = IdleManagerFactory::GetForBrowserContext(profile());
  auto threshold = idle_manager->GetThresholdForTest(kExtensionId);
  ASSERT_EQ(idle_manager->QueryState(threshold), ui::IDLE_STATE_IDLE);

  // Report activity.
  BackgroundScriptExecutor script_executor(profile());
  constexpr char kReportActivityScript[] = R"(chrome.power.reportActivity();)";
  script_executor.BackgroundScriptExecutor::ExecuteScriptAsync(
      kExtensionId, kReportActivityScript,
      BackgroundScriptExecutor::ResultCapture::kNone);

  // The test succeeds if the state goes from idle to active.
  ASSERT_TRUE(catcher.GetNextResult());

  // Test that the actual state is active.
  ASSERT_EQ(idle_manager->QueryState(threshold), ui::IDLE_STATE_ACTIVE);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace extensions
