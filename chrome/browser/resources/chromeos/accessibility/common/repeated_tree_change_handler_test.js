// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/e2e_test_base.js']);

/**
 * Test fixture for repeated_tree_change_handler.js.
 * Note it uses SwitchAccess extension because `repeated_tree_change_handler.js`
 * is only loaded there.
 */
AccessibilityExtensionRepeatedTreeChangeHandlerTest =
    class extends E2ETestBase {
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
    globalThis.RepeatedTreeChangeHandler = imports.RepeatedTreeChangeHandler;
  }
};

TEST_F(
    'AccessibilityExtensionRepeatedTreeChangeHandlerTest',
    'RepeatedTreeChangeHandledOnce', function() {
      this.runWithLoadedDesktop(() => {
        this.handlerCallCount = 0;
        const handler = () => this.handlerCallCount++;

        const repeatedHandler =
            new RepeatedTreeChangeHandler('allTreeChanges', handler);

        // Simulate events being fired.
        repeatedHandler.onChange_();
        repeatedHandler.onChange_();
        repeatedHandler.onChange_();
        repeatedHandler.onChange_();
        repeatedHandler.onChange_();

        // Yield before verifying how many times the handler was called.
        setTimeout(() => assertEquals(this.handlerCallCount, 1), 0);
      });
    });

TEST_F(
    'AccessibilityExtensionRepeatedTreeChangeHandlerTest', 'Predicate',
    function() {
      this.runWithLoadedDesktop(() => {
        this.handlerCallCount = 0;
        const handler = () => this.handlerCallCount++;

        const repeatedHandler = new RepeatedTreeChangeHandler(
            'allTreeChanges', handler,
            {predicate: c => c.type === 'nodeRemoved'});

        // Simulate events being fired.
        repeatedHandler.onChange_({type: 'nodeAdded'});
        repeatedHandler.onChange_({type: 'nodeAdded'});
        repeatedHandler.onChange_({type: 'nodeAdded'});
        repeatedHandler.onChange_({type: 'nodeRemoved'});
        repeatedHandler.onChange_({type: 'nodeRemoved'});
        repeatedHandler.onChange_({type: 'nodeRemoved'});
        repeatedHandler.onChange_({type: 'nodeRemoved'});

        // Verify that nodes that don't satisfy the predicate aren't added to
        // the change stack.
        assertEquals(repeatedHandler.changeStack_.length, 4);

        // Yield before verifying how many times the handler was called.
        setTimeout(() => assertEquals(this.handlerCallCount, 1), 0);
      });
    });
