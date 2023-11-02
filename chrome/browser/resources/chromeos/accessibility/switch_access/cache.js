// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationNode = chrome.automation.AutomationNode;

/**
 * Saves computed values to avoid recalculating them repeatedly.
 *
 * Caches are single-use, and abandoned after the top-level question is answered
 * (e.g. what are all the interesting descendants of this node?)
 */
export class SACache {
  constructor() {
    /** @private {!Map<!AutomationNode, boolean>} */
    this.isActionableMap_ = new Map();

    /** @private {!Map<!AutomationNode, boolean>} */
    this.isGroupMap_ = new Map();

    /** @private {!Map<!AutomationNode, boolean>} */
    this.isInterestingSubtreeMap_ = new Map();
  }

  /** @return {!Map<!AutomationNode, boolean>} */
  get isActionable() {
    return this.isActionableMap_;
  }

  /** @return {!Map<!AutomationNode, boolean>} */
  get isGroup() {
    return this.isGroupMap_;
  }

  /** @return {!Map<!AutomationNode, boolean>} */
  get isInterestingSubtree() {
    return this.isInterestingSubtreeMap_;
  }
}
