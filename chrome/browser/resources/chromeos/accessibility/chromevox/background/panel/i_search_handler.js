// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface for communication from ISearch to ISearchUI.
 */
const AutomationNode = chrome.automation.AutomationNode;

/**
 * An interface implemented by objects that wish to handle events related to
 * incremental search.
 * @interface
 */
export class ISearchHandler {
  /**
   * Called when there are no remaining nodes in the document matching
   * search.
   * @param {!AutomationNode} boundaryNode The last node before reaching either
   * the start or end of the document.
   */
  onSearchReachedBoundary(boundaryNode) {}

  /**
   * Called when search result node changes.
   * @param {!AutomationNode} node The new search result.
   * @param {number} start The index into the name where the search match
   *     starts.
   * @param {number} end The index into the name where the search match ends.
   */
  onSearchResultChanged(node, start, end) {}
}
