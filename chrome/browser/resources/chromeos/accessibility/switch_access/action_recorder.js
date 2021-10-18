// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NodeIdentifier} from './node_identifier.js';
import {SwitchAccess} from './switch_access.js';

/** A class used to record macros and play them back. */
export class ActionRecorder {
  /** @private */
  constructor() {
    /** @private {boolean} */
    this.recording_ = false;
    /** @private {Array<!NodeIdentifier>} */
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
  }

  /** Stops recording actions */
  stop() {
    this.recording_ = false;
  }

  /** @param {!chrome.automation.AutomationNode} node */
  recordNode(node) {
    if (!this.recording_) {
      return;
    }

    this.macro_.push(NodeIdentifier.fromNode(node));
  }

  /** Executes the saved macro */
  executeMacro() {
    // TODO: Implement.
  }
}
