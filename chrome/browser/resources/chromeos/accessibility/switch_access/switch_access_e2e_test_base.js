// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  '../common/testing/assert_additions.js',
  '../common/testing/e2e_test_base.js',
]);

/** Base class for browser tests for Switch Access. */
SwitchAccessE2ETest = class extends E2ETestBase {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_pref_names.h"
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
    // Set some Switch Access prefs so that the os://settings page is not
    // opened (this is done if settings are not configured on first use):
    auto* manager = ash::AccessibilityManager::Get();
    manager->SetSwitchAccessKeysForTest(
        {'1', 'A'}, ash::prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes);
    manager->SetSwitchAccessKeysForTest(
        {'2', 'B'},
        ash::prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes);
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
    await SwitchAccess.ready();
  }

  /**
   * @param {string} id The HTML id of an element.
   * @return {!AutomationNode}
   */
  findNodeById(id) {
    const predicate = node => node.htmlId === id;
    const nodeString = 'node with id "' + id + '"';
    return this.findNodeMatchingPredicate(predicate, nodeString);
  }

  /**
   * @param {string} name The name of the node within the automation tree.
   * @param {string} role The node's role.
   * @return {!AutomationNode}
   */
  findNodeByNameAndRole(name, role) {
    const predicate = node => node.name === name && node.role === role;
    const nodeString = 'node with name "' + name + '" and role ' + role;
    return this.findNodeMatchingPredicate(predicate, nodeString);
  }

  /**
   * @param {function(): boolean} predicate The condition under which the
   *     callback should be fired.
   * @param {function()} callback
   */
  waitForPredicate(predicate, callback) {
    this.listenUntil(predicate, this.desktop_, 'childrenChanged', callback);
  }

  /**
   * @param {!Object} expected
   * @return {!Promise}
   */
  untilFocusIs(expected) {
    const doesMatch = expected => {
      const newNode = Navigator.byItem.node_;
      const automationNode = newNode.automationNode || {};
      return (!expected.instance || newNode instanceof expected.instance) &&
          (!expected.role || expected.role === automationNode.role) &&
          (!expected.name || expected.name === automationNode.name) &&
          (!expected.className ||
           expected.className === automationNode.className);
    };

    let didResolve = false;
    let lastFocusChangeTime = new Date();
    const id = setInterval(() => {
      if (didResolve) {
        clearInterval(id);
        return;
      }

      if ((new Date() - lastFocusChangeTime) < 3000) {
        return;
      }

      console.log(
          `\nStill waiting for expectation: ${JSON.stringify(expected)}\n` +
          `Focus is: ${Navigator.byItem.node_.debugString()}`);
    }, 1000);
    return new Promise(resolve => {
      if (doesMatch(expected)) {
        didResolve = true;
        resolve(Navigator.byItem.node_);
        return;
      }
      this.addCallbackPostMethod(Navigator.byItem, 'setNode_', node => {
        lastFocusChangeTime = new Date();
        if (doesMatch(expected)) {
          didResolve = true;
          resolve(Navigator.byItem.node_);
        }
      }, () => doesMatch(expected));
    });
  }
};
