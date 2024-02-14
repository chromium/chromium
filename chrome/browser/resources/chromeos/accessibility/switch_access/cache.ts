// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

type AutomationNode = chrome.automation.AutomationNode;
type CacheMap = Map<AutomationNode, boolean>;

/**
 * Saves computed values to avoid recalculating them repeatedly.
 *
 * Caches are single-use, and abandoned after the top-level question is answered
 * (e.g. what are all the interesting descendants of this node?)
 */
export class SACache {
  readonly isActionable: CacheMap = new Map();
  readonly isGroup: CacheMap = new Map();
  readonly isInterestingSubtree: CacheMap = new Map();
}

TestImportManager.exportForTesting(SACache);
