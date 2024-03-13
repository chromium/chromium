// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/idle/idle_manager.h"
#include "extensions/browser/api/idle/idle_manager_factory.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/base/idle/scoped_set_idle_state.h"

namespace extensions {

namespace {

constexpr char kExtensionRelativePath[] = "power/report_activity";
constexpr char kExtensionId[] = "dibbenaepdnglcjpgjmnefmjccpinang";

}  // namespace

using ContextType = ExtensionBrowserTest::ContextType;

class PowerApiTest : public ExtensionApiTest,
                     public testing::WithParamInterface<ContextType> {
 public:
  PowerApiTest() : ExtensionApiTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         PowerApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         PowerApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Verifies that chrome.power.reportActivity() correctly reports a user activity
// by observing idle states.
IN_PROC_BROWSER_TEST_P(PowerApiTest,
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

}  // namespace extensions
