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
// eslint-disable-next-line @typescript-eslint/naming-convention
export class SACache {
  readonly isActionable: Map<AutomationNode, boolean> = new Map();
  readonly isGroup: Map<AutomationNode, boolean> = new Map();
  readonly isInterestingSubtree: Map<AutomationNode, boolean> = new Map();
}
