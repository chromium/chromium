// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A class that provides test support for C++ tests of HTML pages.
 * The C++ end is implemented in ash::AutomationTestUtils.
 */
class AutomationTestSupport {
  constructor() {
    this.desktop_ = null;
    this.init_();
  }

  /** @private */
  async init_() {
    this.desktop_ =
        await new Promise(resolve => chrome.automation.getDesktop(resolve));
    window.domAutomationController.send('ready');
  }

  /**
   * Gets the bounds for the automation node with the given `name` and
   * `role.
   * @param {string} name
   * @param {string} role
   */
  getBoundsForNode(name, role) {
    const node = this.desktop_.find({role, attributes: {name}});
    if (!node || !node.location) {
      // Failed.
      return;
    }
    window.domAutomationController.send(`${node.location.left},${
        node.location.top},${node.location.width},${node.location.height}`);
  }
}

globalThis.automationTestSupport = new AutomationTestSupport();
