// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DropPosition, IncognitoAvailability} from './constants.js';

/**
 * @fileoverview Closure typedefs for Bookmarks.
 */

/**
 * A normalized version of chrome.bookmarks.BookmarkTreeNode.
 * @typedef {{
 *   id: string,
 *   parentId: (string|undefined),
 *   url: (string|undefined),
 *   title: string,
 *   dateAdded: (number|undefined),
 *   dateGroupModified: (number|undefined),
 *   unmodifiable: (string|undefined),
 *   children: (!Array<string>|undefined),
 * }}
 */
export let BookmarkNode;

/**
 * @typedef {!Object<string, BookmarkNode>}
 */
export let NodeMap;

/**
 * @typedef {{
 *   items: !Set<string>,
 *   anchor: ?string,
 * }}
 *
 * |items| is used as a set and all values in the map are true.
 */
export let SelectionState;

/**
 * Note:
 * - If |results| is null, it means no search results have been returned. This
 *   is different to |results| being [], which means the last search returned 0
 *   results.
 * - |term| is the last search that was performed by the user, and |results| are
 *   the last results that were returned from the backend. We don't clear
 *   |results| on incremental searches, meaning that |results| can be 'stale'
 *   data from a previous search term (while |inProgress| is true). If you need
 *   to know the exact search term used to generate |results|, you'll need to
 *   add a new field to the state to track it (eg, SearchState.resultsTerm).
 * @typedef {{
 *   term: string,
 *   inProgress: boolean,
 *   results: ?Array<string>,
 * }}
 */
export let SearchState;

/** @typedef {!Map<string, boolean>} */
export let FolderOpenState;

/**
 * @typedef {{
 *   canEdit: boolean,
 *   incognitoAvailability: IncognitoAvailability,
 * }}
 */
export let PreferencesState;

/**
 * @typedef {{
 *   nodes: NodeMap,
 *   selectedFolder: string,
 *   folderOpenState: FolderOpenState,
 *   prefs: PreferencesState,
 *   search: SearchState,
 *   selection: SelectionState,
 * }}
 */
export let BookmarksPageState;

/** @typedef {{element: BookmarkElement, position: DropPosition}} */
export let DropDestination;

export class BookmarkElement extends HTMLElement {
  constructor() {
    super();

    /** @type {string} */
    this.itemId = '';
  }

  /** @return {HTMLElement} */
  getDropTarget() {}
}

export class DragData {
  constructor() {
    /** @type {Array<BookmarkTreeNode>} */
    this.elements = null;

    /** @type {boolean} */
    this.sameProfile = false;
  }
}
