// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The top-level class for the Switch Access accessibility feature. Handles
 * initialization and small matters that don't fit anywhere else in the
 * codebase.
 */
class SwitchAccess {
  static initialize() {
    SwitchAccess.instance = new SwitchAccess();

    chrome.automation.getDesktop((desktop) => {
      // NavigationManager must be initialized first.
      NavigationManager.initialize(desktop);

      Commands.initialize();
      KeyboardRootNode.startWatchingVisibility();
      SwitchAccessPreferences.initialize();
    });
  }

  /** @private */
  constructor() {
    /**
     * Feature flag controlling improvement of text input capabilities.
     * @private {boolean}
     */
    this.enableImprovedTextInput_ = false;

    chrome.commandLinePrivate.hasSwitch(
        'enable-experimental-accessibility-switch-access-text', (result) => {
          this.enableImprovedTextInput_ = result;
        });
  }

  /**
   * Returns whether or not the feature flag
   * for improved text input is enabled.
   * @return {boolean}
   */
  improvedTextInputEnabled() {
    return this.enableImprovedTextInput_;
  }

  /**
   * Helper function to robustly find a node fitting a given FindParams, even if
   * that node has not yet been created.
   * Used to find the menu and back button.
   * @param {!chrome.automation.FindParams} findParams
   * @param {!function(!AutomationNode): void} foundCallback
   */
  static findNodeMatching(findParams, foundCallback) {
    const desktop = NavigationManager.desktopNode;
    // First, check if the node is currently in the tree.
    let node = desktop.find(findParams);
    if (node) {
      foundCallback(node);
      return;
    }
    // If it's not currently in the tree, listen for changes to the desktop
    // tree.
    const eventHandler = new EventHandler(
        desktop, chrome.automation.EventType.CHILDREN_CHANGED,
        null /** callback */);

    const onEvent = (event) => {
      if (event.target.matches(findParams)) {
        // If the event target is the node we're looking for, we've found it.
        eventHandler.stop();
        foundCallback(event.target);
      } else if (event.target.children.length > 0) {
        // Otherwise, see if one of its children is the node we're looking for.
        node = event.target.find(findParams);
        if (node) {
          eventHandler.stop();
          foundCallback(node);
        }
      }
    };

    eventHandler.setCallback(onEvent);
    eventHandler.start();
  }

  /*
   * Creates and records the specified error.
   * @param {SAConstants.ErrorType} errorType
   * @param {string} errorString
   * @param {boolean} shouldRecover
   * @return {!Error}
   */
  static error(errorType, errorString, shouldRecover = false) {
    if (shouldRecover) {
      setTimeout(NavigationManager.moveToValidNode, 0);
    }
    const errorTypeCountForUMA = Object.keys(SAConstants.ErrorType).length;
    chrome.metricsPrivate.recordEnumerationValue(
        'Accessibility.CrosSwitchAccess.Error', errorType,
        errorTypeCountForUMA);
    return new Error(errorString);
  }
}
