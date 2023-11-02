// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationNode = chrome.automation.AutomationNode;
const AutomationEvent = chrome.automation.AutomationEvent;
const EventType = chrome.automation.EventType;

import {AutomationPredicate} from '../../common/automation_predicate.js';
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

    const desktop =
        await new Promise(resolve => chrome.automation.getDesktop(resolve));

    const focus =
        await new Promise(resolve => chrome.automation.getFocus(resolve));
    if (focus && AutomationPredicate.editText(focus)) {
      this.editableNode_ = focus;
    }

    if (!this.eventHandler_) {
      this.eventHandler_ = new EventHandler(
          [], EventType.FOCUS, event => this.onFocusChanged_(event));
    }
    this.eventHandler_.setNodes(desktop);
    this.eventHandler_.start();

    this.active_ = true;
  }

  /** @private */
  deactivate_() {
    this.eventHandler_.stop();
    this.eventHandler_ = null;
    this.active_ = false;
    this.editableNode_ = null;
  }

  /**
   * Saves the focused node if it's an editable.
   * @param {!AutomationEvent} event
   * @private
   */
  onFocusChanged_(event) {
    const node = event.target;
    if (!node || !AutomationPredicate.editText(node)) {
      this.editableNode_ = null;
      return;
    }

    this.editableNode_ = node;
  }

  /** @return {?AutomationNode} */
  getEditableNode() {
    return this.editableNode_;
  }

  /** @return {boolean} */
  isReadyForTesting() {
    return this.active_ && this.editableNode_ !== null;
  }
}

/**
 * @const {number}
 * @private
 */
FocusHandler.DEACTIVATE_TIMEOUT_MS_ = 45 * 1000;
