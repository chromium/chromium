// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FocusRingManager} from './focus_ring_manager.js';
import {NodeIdentifier} from './node_identifier.js';
import {SwitchAccess} from './switch_access.js';

/** A class used to record macros and play them back. */
export class ActionRecorder {
  /** @private */
  constructor() {
    /** @private {boolean} */
    this.recording_ = false;
    /** @private {!Array<!NodeIdentifier>} */
    this.macro_ = [];
  }

  /** @return {!ActionRecorder} */
  static get instance() {
    if (!SwitchAccess.instance.multistepAutomationFeaturesEnabled()) {
      throw new Error(
          'Multistep automation flag must be enabled to access ActionRecorder');
    }

    if (!ActionRecorder.instance_) {
      ActionRecorder.instance_ = new ActionRecorder();
    }
    return ActionRecorder.instance_;
  }

  /** Starts recording actions */
  start() {
    this.recording_ = true;
    this.macro_ = [];
    FocusRingManager.setIsRecording(true);
  }

  /** Stops recording actions */
  stop() {
    this.recording_ = false;
    FocusRingManager.setIsRecording(false);
  }

  /** @param {!chrome.automation.AutomationNode} node */
  recordNode(node) {
    if (!this.recording_) {
      return;
    }

    if (node.className === 'SwitchAccessBackButtonView' ||
        node.className === 'SwitchAccessMenuButton') {
      // Do not record actions on the back button or menu buttons.
      return;
    }

    this.macro_.push(NodeIdentifier.fromNode(node));
  }

  /** Executes the saved macro */
  async executeMacro() {
    const desktop =
        await new Promise(resolve => chrome.automation.getDesktop(resolve));
    for (const identifier of this.macro_) {
      // Wait for stable state.
      // TODO: replace this with something more substantive e.g. a focus or
      // page load listener.
      await this.sleep();
      // Find node.
      const node = this.find_(desktop, identifier);
      if (!node) {
        return;
      }

      // Focus node with Switch Access.
      node.focus();

      // TODO: draw focus ring around node.

      // Perform 'Select' on the node.
      node.doDefault();
    }
  }

  sleep() {
    return new Promise(resolve => setTimeout(resolve, 1000));
  }

  /**
   * Searches through the tree for a node that matches `target`.
   * @param {!chrome.automation.AutomationNode} root
   * @param {!NodeIdentifier} target
   * @return {chrome.automation.AutomationNode}
   * @private
   */
  find_(root, target) {
    const nodes = [];
    nodes.push(root);
    while (nodes.length !== 0) {
      const current = nodes.shift();
      // Goal test.
      if (NodeIdentifier.fromNode(current).equals(target)) {
        return current;
      }

      for (const child of current.children) {
        nodes.push(child);
      }
    }

    return null;
  }
}
