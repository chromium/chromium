// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type AutomationNode = chrome.automation.AutomationNode;
type AutomationEvent = chrome.automation.AutomationEvent;
import EventType = chrome.automation.EventType;

import {AutomationPredicate} from '/common/automation_predicate.js';
import {AsyncUtil} from '/common/async_util.js';
import {EventHandler} from '/common/event_handler.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

export class FocusHandler {
  private active_ = false;
  /** The currently focused editable node. */
  private editableNode_: AutomationNode|null = null;
  private deactivateTimeoutId_: number|null = null;
  private eventHandler_: EventHandler|null = null;
  private onActiveChangedForTesting_: (() => void)|null = null;
  private onEditableNodeChangedForTesting_: (() => void)|null = null;
  private onFocusChangedForTesting_: (() => void)|null = null;

  /**
   * Starts listening to focus events and sets a timeout to deactivate after
   * a certain amount of inactivity.
   */
  async refresh(): Promise<void> {
    if (this.deactivateTimeoutId_ !== null) {
      clearTimeout(this.deactivateTimeoutId_);
    }
    this.deactivateTimeoutId_ = setTimeout(
        () => this.deactivate_(), FocusHandler.DEACTIVATE_TIMEOUT_MS_);

    await this.activate_();
  }

  /** Gets the current focus and starts listening for focus events. */
  private async activate_(): Promise<void> {
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

  private deactivate_(): void {
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    this.eventHandler_!.stop();
    this.eventHandler_ = null;
    this.setActive_(false);
    this.setEditableNode_(null);
  }

  /** Saves the focused node if it's an editable. */
  private onFocusChanged_(event: AutomationEvent): void {
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

  getEditableNode(): AutomationNode|null {
    return this.editableNode_;
  }

  private setActive_(value: boolean): void {
    this.active_ = value;
    if (this.onActiveChangedForTesting_) {
      this.onActiveChangedForTesting_();
    }
  }

  private setEditableNode_(node: AutomationNode|null): void {
    this.editableNode_ = node;
    if (this.onEditableNodeChangedForTesting_) {
      this.onEditableNodeChangedForTesting_();
    }
  }

  isReadyForTesting(expectedClassName: string): boolean {
    return this.active_ && this.editableNode_ !== null &&
        this.editableNode_.className === expectedClassName;
  }
}

export namespace FocusHandler {
  export const DEACTIVATE_TIMEOUT_MS_ = 45 * 1000;
}

TestImportManager.exportForTesting(FocusHandler);
