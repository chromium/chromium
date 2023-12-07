// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type AutomationNode = chrome.automation.AutomationNode;

/**
 * Saves computed values to avoid recalculating them repeatedly.
 *
 * Caches are single-use, and abandoned after the top-level question is answered
 * (e.g. what are all the interesting descendants of this node?)
 */
export class SACache {
  private isActionableMap: Map<AutomationNode, boolean> = new Map();
  private isGroupMap: Map<AutomationNode, boolean> = new Map();
  private isInterestingSubtreeMap: Map<AutomationNode, boolean> = new Map();

  get isActionable(): Map<AutomationNode, boolean> {
    return this.isActionableMap;
  }

  get isGroup(): Map<AutomationNode, boolean> {
    return this.isGroupMap;
  }

  get isInterestingSubtree(): Map<AutomationNode, boolean> {
    return this.isInterestingSubtreeMap;
  }
}
