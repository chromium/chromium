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
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
  base::OnceClosure load_cb =
      base::BindOnce(&ash::AccessibilityManager::SetSwitchAccessEnabled,
          base::Unretained(ash::AccessibilityManager::Get()),
          true);
    `);
    super.testGenPreambleCommon('kSwitchAccessExtensionId');
  }

  /**
   * @param {string} id The HTML id of an element.
   * @return {!AutomationNode}
   */
  findNodeById(id) {
    const predicate = node => node.htmlAttributes.id === id;
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
    this.listenUntil(
        predicate, Navigator.byItem.desktopNode, 'childrenChanged', callback);
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
      const original = Navigator.byItem.setNode_.bind(Navigator.byItem);
      Navigator.byItem.setNode_ = node => {
        original(node);
        lastFocusChangeTime = new Date();
        if (doesMatch(expected)) {
          Navigator.byItem.setNode_ = original;
          didResolve = true;
          resolve(Navigator.byItem.node_);
        }
      };
    });
  }
};
