// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationNode = chrome.automation.AutomationNode;
const AutomationEvent = chrome.automation.AutomationEvent;
const EventType = chrome.automation.EventType;

import {AutomationPredicate} from '../../common/automation_predicate.js';
import {AsyncUtil} from '../../common/async_util.js';
import {EventHandler} from '../../common/event_handler.js';

export class FocusHandler {
  constructor() {
    /** @private {boolean} */
    this.active_ = false;

    /**
     * The currently focused editable node.
     * @private {?AutomationNode}
     */
    this.editableNode_ = null;

    /** @private {?number} */
    this.deactivateTimeoutId_ = null;

    /** @private {?EventHandler} */
    this.eventHandler_ = null;

    /** @private {?function(): void} */
    this.onActiveChangedForTesting_ = null;
    /** @private {?function(): void} */
    this.onEditableNodeChangedForTesting_ = null;
    /** @private {?function(): void} */
    this.onFocusChangedForTesting_ = null;
  }

  /**
   * Starts listening to focus events and sets a timeout to deactivate after
   * a certain amount of inactivity.
   * @return {!Promise}
   */
  async refresh() {
    if (this.deactivateTimeoutId_ !== null) {
      clearTimeout(this.deactivateTimeoutId_);
    }
    this.deactivateTimeoutId_ = setTimeout(
        () => this.deactivate_(), FocusHandler.DEACTIVATE_TIMEOUT_MS_);

    await this.activate_();
  }

  /**
   * Gets the current focus and starts listening for focus events.
   * @private
   * @return {!Promise}
   */
  async activate_() {
    if (this.active_) {
      return;
    }

    const desktop = await AsyncUtil.getDesktop();
    const focus = await AsyncUtil.getFocus();
    if (focus && AutomationPredicate.editText(focus)) {
      this.setEditableNode_(focus);
    }

    if (!this.eventHandler_) {
      this.eventHandler_ = new EventHandler(
          [], EventType.FOCUS, event => this.onFocusChanged_(event));
    }
    this.eventHandler_.setNodes(desktop);
    this.eventHandler_.start();

    this.setActive_(true);
  }

  /** @private */
  deactivate_() {
    this.eventHandler_.stop();
    this.eventHandler_ = null;
    this.setActive_(false);
    this.setEditableNode_(null);
  }

  /**
   * Saves the focused node if it's an editable.
   * @param {!AutomationEvent} event
   * @private
   */
  onFocusChanged_(event) {
    const node = event.target;
    if (!node || !AutomationPredicate.editText(node)) {
      this.setEditableNode_(null);
      return;
    }

    this.setEditableNode_(node);

    if (this.onFocusChangedForTesting_) {
      this.onFocusChangedForTesting_();
    }
  }

  /** @return {?AutomationNode} */
  getEditableNode() {
    return this.editableNode_;
  }

  /**
   * @param {boolean} value
   * @private
   */
  setActive_(value) {
    this.active_ = value;
    if (this.onActiveChangedForTesting_) {
      this.onActiveChangedForTesting_();
    }
  }

  /**
   * @param {AutomationNode} node
   * @private
   */
  setEditableNode_(node) {
    this.editableNode_ = node;
    if (this.onEditableNodeChangedForTesting_) {
      this.onEditableNodeChangedForTesting_();
    }
  }

  /**
   * @param {string} expectedClassName
   * @return {boolean}
   */
  isReadyForTesting(expectedClassName) {
    return this.active_ && this.editableNode_ !== null &&
        this.editableNode_.className === expectedClassName;
  }
}

/**
 * @const {number}
 * @private
 */
FocusHandler.DEACTIVATE_TIMEOUT_MS_ = 45 * 1000;
