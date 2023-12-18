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
    this.notifyCcTests_('ready');
  }

  /**
   * Waits for the page with the given `url` to finish loading.
   * @param {string} url
   */
  async waitForPageLoad(url) {
    const findParams = {
      role: 'rootWebArea',
      attributes: {url, 'docLoaded': true},
    };
    await this.findNode_(findParams);
    this.notifyCcTests_('ready');
  }

  /**
   * Waits for the page with the given `url` to exist, then
   * gets its bounds.
   * @param {string} url
   */
  async getBoundsForRootWebArea(url) {
    const findParams = {
      role: 'rootWebArea',
      attributes: {url},
    };
    await this.getBoundsForNodeWithParams_(findParams);
  }

  /**
   * Gets the bounds for the automation node with the given `name` and
   * `role`. Waits for the node to exist if it does not yet.
   * @param {string} name
   * @param {string} role
   */
  async getBoundsForNode(name, role) {
    const findParams = {role, attributes: {name}};
    await this.getBoundsForNodeWithParams_(findParams);
  }

  /**
   * Gets the bounds for the automation node with the given `className`.
   * Waits for the node to exist if it does not yet.
   * @param {string} className
   */
  async getBoundsForNodeByClassName(className) {
    const findParams = {attributes: {className}};
    await this.getBoundsForNodeWithParams_(findParams);
  }

  /**
   * Sets focus on the automation node with the given `name` and
   * `role`. Waits for the node to exist if it does not yet.
   * @param {string} name
   * @param {string} role
   */
  async setFocusOnNode(name, role) {
    const findParams = {role, attributes: {name}};
    const node = await this.findNode_(findParams);
    if (!node) {
      // Failed, node never existed.
      console.error('Failed to find node', name, 'with role', role);
      return;
    }
    node.focus();
    this.notifyCcTests_('ready');
  }

  /**
   * Checks if an automation node with the given `name` and
   * `role` exists in the desktop tree, without waiting.
   * @param {string} name
   * @param {string} role
   */
  nodeExistsNoWait(name, role) {
    const findParams = {role, attributes: {name}};
    const node = this.desktop_.find(findParams);
    if (node) {
      this.notifyCcTests_('true');
    } else {
      this.notifyCcTests_('false');
    }
  }

  /**
   * Does the default action on the node with the given `name` and `role` after
   * waiting for that node to exist.
   * @param {string} name
   * @param {string} role
   */
  doDefault(name, role) {
    const findParams = {role, attributes: {name}};
    const node = this.desktop_.find(findParams);
    if (node) {
      node.doDefault();
      this.notifyCcTests_('ready');
    }
  }

  /**
   * @param {chrome.automation.FindParams} findParams
   * @private
   */
  async getBoundsForNodeWithParams_(findParams) {
    const node = await this.findNode_(findParams);
    if (!node || !node.location) {
      // Failed.
      return;
    }
    this.notifyCcTests_(`${node.location.left},${node.location.top},${
        node.location.width},${node.location.height}`);
  }

  /**
   * Finds the automation node with the given `findParams`. Waits
   * for the node to exist if it does not yet.
   * @param {chrome.automation.FindParams} findParams
   * @return {chrome.automation.AutomationNode}
   * @private
   */
  async findNode_(findParams) {
    const nodes = await this.findNumNodes_(findParams, 1);
    return nodes[0];
  }

  /**
   * Finds at least `minToFind` of the automation nodes matching the given
   * `findParams`.
   * @param {chrome.automation.FindParams} findParams
   * @param {Number} minToFind
   * @return {!Array<!chrome.automation.AutomationNode}
   * @private
   */
  async findNumNodes_(findParams, minToFind) {
    const nodes = this.desktop_.findAll(findParams);
    if (nodes && nodes.length >= minToFind) {
      return nodes;
    }
    // If there weren't enough found yet, wait for them to show up.
    return await new Promise(resolve => {
      const listener = event => {
        const nodes = this.desktop_.findAll(findParams);
        if (nodes && nodes.length >= minToFind) {
          this.desktop_.removeEventListener(
              chrome.automation.EventType.LOAD_COMPLETE, listener, true);
          this.desktop_.removeEventListener(
              chrome.automation.EventType.CHILDREN_CHANGED, listener, true);
          resolve(nodes);
        }
      };
      this.desktop_.addEventListener(
          chrome.automation.EventType.LOAD_COMPLETE, listener, true);
      this.desktop_.addEventListener(
          chrome.automation.EventType.CHILDREN_CHANGED, listener, true);
    });
  }

  /**
   * Waits for a chrome.automation.EventType.TEXT_SELECTION_CHANGED event to be
   * fired on the desktop node.
   */
  async waitForTextSelectionChangedEvent() {
    await this.waitForEventHelper_(
        chrome.automation.EventType.TEXT_SELECTION_CHANGED);
  }

  /**
   * Waits for a chrome.automation.EventType.VALUE_CHANGED event to be fired on
   * the desktop node.
   */
  async waitForValueChangedEvent() {
    await this.waitForEventHelper_(chrome.automation.EventType.VALUE_CHANGED);
  }

  /**
   * Waits for a chrome.automation.EventType.CHILDREN_CHANGED event to be fired
   * on the desktop node.
   */
  async waitForChildrenChangedEvent() {
    await this.waitForEventHelper_(
        chrome.automation.EventType.CHILDREN_CHANGED);
  }

  /**
   * Waits for the browser to have `num` tabs with name `name`.
   */
  async waitForNumTabsWithName(num, name) {
    const findParams = {
      role: 'tab',
      attributes: {name, className: 'Tab'},
    };
    // This will not return until it finds at least num.
    const nodes = await this.findNumNodes_(findParams, num);
    if (nodes.length > num) {
      // Fail if we've found too many.
      console.error(
          'Error: found ' + nodes.length + ' tabs with name ' + name +
          ', expected only ' + num);
    } else {
      this.notifyCcTests_('ready');
    }
  }

  /** @param {string} className */
  async getValueForNodeWithClassName(className) {
    const findParams = {attributes: {className}};
    const node = await this.findNode_(findParams);
    if (!node || !node.value) {
      return;
    }

    this.notifyCcTests_(node.value);
  }

  /**
   * @param {string} className
   * @param {string} value
   */
  async waitForNodeWithClassNameAndValue(className, value) {
    const findParams = {attributes: {className, value}};
    const node = await this.findNode_(findParams);
    this.notifyCcTests_('ready');
  }

  /**
   * @param {chrome.automation.EventType} event
   * @private
   */
  async waitForEventHelper_(event) {
    const desktop = await new Promise(resolve => {
      chrome.automation.getDesktop(d => resolve(d));
    });
    await new Promise(resolve => {
      desktop.addEventListener(event, resolve);
    });
    this.notifyCcTests_('ready');
  }

  /**
   * @param {string} The result to send to the C++ tests.
   * @private
   */
  notifyCcTests_(params) {
    window.domAutomationController.send(params);
  }
}

globalThis.automationTestSupport = new AutomationTestSupport();
