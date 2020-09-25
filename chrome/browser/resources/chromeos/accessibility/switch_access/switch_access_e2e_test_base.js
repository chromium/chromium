// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  '../common/testing/assert_additions.js', '../common/testing/e2e_test_base.js'
]);

/** Base class for browser tests for Switch Access. */
SwitchAccessE2ETest = class extends E2ETestBase {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
#include "ash/keyboard/ui/keyboard_util.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`
  base::Closure load_cb =
      base::Bind(&chromeos::AccessibilityManager::SetSwitchAccessEnabled,
          base::Unretained(chromeos::AccessibilityManager::Get()),
          true);
  chromeos::AccessibilityManager::Get()->SetSwitchAccessEnabled(true);
  WaitForExtension(extension_misc::kSwitchAccessExtensionId, load_cb);
    `);
  }

  /**
   * @param {string} id The HTML id of an element.
   * @return {!AutomationNode}
   */
  findNodeById(id) {
    const predicate = (node) => node.htmlAttributes.id === id;
    const nodeString = 'node with id "' + id + '"';
    return this.findNodeMatchingPredicate(predicate, nodeString);
  }

  /**
   * @param {string} name The name of the node within the automation tree.
   * @param {string} role The node's role.
   * @return {!AutomationNode}
   */
  findNodeByNameAndRole(name, role) {
    const predicate = (node) => node.name === name && node.role === role;
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
        predicate, NavigationManager.desktopNode, 'childrenChanged', callback);
  }
};
