// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface for communication from ISearch to ISearchUI.
 */
type AutomationNode = chrome.automation.AutomationNode;

/**
 * An interface implemented by objects that wish to handle events related to
 * incremental search.
 * TODO(b/346347267): Convert to an interface once typescript migration is done.
 */
export abstract class ISearchHandler {
  /**
   * Called when there are no remaining nodes in the document matching
   * search.
   * @param boundaryNode The last node before reaching either the start or end
   *     of the document.
   */
  abstract onSearchReachedBoundary(boundaryNode: AutomationNode): void;

  /**
   * Called when search result node changes.
   * @param node The new search result.
   * @param start The index into the name where the search match starts.
   * @param end The index into the name where the search match ends.
   */
  abstract onSearchResultChanged(
      node: AutomationNode, start: number, end: number): void;
}
