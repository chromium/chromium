// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/e2e_test_base.js']);

/**
 * Test fixture for repeated_event_handler.js.
 * Note it uses SwitchAccess extension because `repeated_event_handler.js` is
 * only loaded there.
 */
AccessibilityExtensionRepeatedEventHandlerTest = class extends E2ETestBase {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/accessibility/accessibility_controller.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
    auto* controller = ash::AccessibilityController::Get();
    controller->DisableSwitchAccessDisableConfirmationDialogTesting();
    // Don't show the dialog saying Switch Access was enabled.
    controller->DisableSwitchAccessEnableNotificationTesting();
    base::OnceClosure load_cb =
        base::BindOnce(&ash::AccessibilityManager::SetSwitchAccessEnabled,
            base::Unretained(ash::AccessibilityManager::Get()),
            true);
    `);
    super.testGenPreambleCommon('kSwitchAccessExtensionId');
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    const imports = TestImportManager.getImports();
    globalThis.EventGenerator = imports.EventGenerator;
    globalThis.KeyCode = imports.KeyCode;
    globalThis.RepeatedEventHandler = imports.RepeatedEventHandler;
  }
};

AX_TEST_F(
    'AccessibilityExtensionRepeatedEventHandlerTest',
    'RepeatedEventHandledOnce', async function() {
      const root = await this.runWithLoadedTree('');
      this.handlerCallCount = 0;
      const handler = () => this.handlerCallCount++;

      const repeatedHandler = new RepeatedEventHandler(root, 'focus', handler);

      // Simulate events being fired.
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();

      // Yield before verify how many times the handler was called.
      setTimeout(
          this.newCallback(() => assertEquals(this.handlerCallCount, 1)), 0);
    });

AX_TEST_F(
    'AccessibilityExtensionRepeatedEventHandlerTest',
    'NoEventsHandledAfterStopListening', async function() {
      const root = await this.runWithLoadedTree('');
      this.handlerCallCount = 0;
      const handler = () => this.handlerCallCount++;

      const repeatedHandler = new RepeatedEventHandler(root, 'focus', handler);

      // Simulate events being fired.
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();
      repeatedHandler.onEvent_();

      repeatedHandler.stop();

      // Yield before verifying how many times the handler was called.
      setTimeout(
          this.newCallback(() => assertEquals(this.handlerCallCount, 0)), 0);
    });
