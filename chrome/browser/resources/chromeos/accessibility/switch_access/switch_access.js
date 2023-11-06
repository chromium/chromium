// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncUtil} from '../common/async_util.js';
import {EventHandler} from '../common/event_handler.js';
import {FlagName, Flags} from '../common/flags.js';

import {Navigator} from './navigator.js';
import {KeyboardRootNode} from './nodes/keyboard_node.js';
import {ErrorType, Mode} from './switch_access_constants.js';

const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const FindParams = chrome.automation.FindParams;
const RoleType = chrome.automation.RoleType;

let readyCallback;
const readyPromise = new Promise(resolve => readyCallback = resolve);

/**
 * The top-level class for the Switch Access accessibility feature. Handles
 * initialization and small matters that don't fit anywhere else in the
 * codebase.
 */
export class SwitchAccess {
  /** @private */
  constructor() {
    /* @private {!Mode} */
    this.mode_ = Mode.ITEM_SCAN;
  }

  /** @param {!AutomationNode} desktop */
  static async init(desktop) {
    if (SwitchAccess.instance) {
      throw new Error('Cannot create two SwitchAccess.instances');
    }
    SwitchAccess.instance = new SwitchAccess();

    const currentFocus = await AsyncUtil.getFocus();
    await SwitchAccess.instance.waitForFocus_(desktop, currentFocus);
  }

  /** Starts Switch Access behavior. */
  static start() {
    KeyboardRootNode.startWatchingVisibility();
    readyCallback();
  }

  /** @return {!Promise} */
  static async ready() {
    return readyPromise;
  }

  /**
   * Returns whether or not the feature flag
   * for improved text input is enabled.
   * @return {boolean}
   */
  static improvedTextInputEnabled() {
    return Flags.isEnabled(FlagName.SWITCH_ACCESS_TEXT);
  }

  /** @return {!Mode} */
  static get mode() {
    return SwitchAccess.instance.mode_;
  }

  /** @param {!Mode} newMode */
  static set mode(newMode) {
    SwitchAccess.instance.mode_ = newMode;
  }

  /**
   * Helper function to robustly find a node fitting a given FindParams, even if
   * that node has not yet been created.
   * Used to find the menu and back button.
   * @param {!FindParams} findParams
   * @param {!function(!AutomationNode): void} foundCallback
   */
  static findNodeMatching(findParams, foundCallback) {
    const desktop = Navigator.byItem.desktopNode;
    // First, check if the node is currently in the tree.
    let node = desktop.find(findParams);
    if (node) {
      foundCallback(node);
      return;
    }
    // If it's not currently in the tree, listen for changes to the desktop
    // tree.
    const eventHandler = new EventHandler(
        desktop, EventType.CHILDREN_CHANGED, null /** callback */);

    const onEvent = event => {
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

  /**
   * Creates and records the specified error.
   * @param {ErrorType} errorType
   * @param {string} errorString
   * @param {boolean} shouldRecover
   * @return {!Error}
   */
  static error(errorType, errorString, shouldRecover = false) {
    if (shouldRecover) {
      setTimeout(Navigator.byItem.moveToValidNode.bind(Navigator.byItem), 0);
    }
    const errorTypeCountForUMA = Object.keys(ErrorType).length;
    chrome.metricsPrivate.recordEnumerationValue(
        'Accessibility.CrosSwitchAccess.Error',
        /** @type {number} */ (errorType), errorTypeCountForUMA);
    return new Error(errorString);
  }

  /**
   * @param {!AutomationNode} desktop
   * @param {AutomationNode} currentFocus
   * @private
   */
  async waitForFocus_(desktop, currentFocus) {
    return new Promise(resolve => {
      // Focus is available. Finish init without waiting for further events.
      // Disallow web view nodes, which indicate a root web area is still
      // loading and pending focus.
      if (currentFocus && currentFocus.role !== RoleType.WEB_VIEW) {
        resolve();
        return;
      }

      // Wait for the focus to be sent. If |currentFocus| was undefined, this is
      // guaranteed. Otherwise, also set a timed callback to ensure we do
      // eventually init.
      let callbackId = 0;
      const listener = maybeEvent => {
        if (maybeEvent && maybeEvent.target.role === RoleType.WEB_VIEW) {
          return;
        }

        desktop.removeEventListener(EventType.FOCUS, listener, false);
        clearTimeout(callbackId);

        resolve();
      };

      desktop.addEventListener(EventType.FOCUS, listener, false);
      callbackId = setTimeout(listener, 5000);
    });
  }
}

/** @type {SwitchAccess} */
SwitchAccess.instance;
