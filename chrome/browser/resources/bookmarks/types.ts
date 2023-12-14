// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DropPosition, IncognitoAvailability, MenuSource} from './constants.js';

/**
 * @fileoverview Closure typedefs for Bookmarks.
 */

// A normalized version of chrome.bookmarks.BookmarkTreeNode.
export interface BookmarkNode {
  id: string;
  title: string;
  parentId?: string;
  url?: string;
  dateAdded?: number;
  dateLastUsed?: number;
  dateGroupModified?: number;
  unmodifiable?: string;
  children?: string[];
}

export interface ObjectMap<Type> {
  [index: string]: Type;
}

export type NodeMap = ObjectMap<BookmarkNode>;

// |items| is used as a set and all values in the map are true.
export interface SelectionState {
  items: Set<string>;
  anchor?: string|null;
}

export interface OpenCommandMenuDetail {
  x?: number;
  y?: number;
  source: MenuSource;
  targetId?: string;
  targetElement?: HTMLElement;
}

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
 */
export interface SearchState {
  term: string;
  inProgress: boolean;
  results: string[]|null;
}

export type FolderOpenState = Map<string, boolean>;

export interface PreferencesState {
  canEdit: boolean;
  incognitoAvailability: IncognitoAvailability;
}

export interface BookmarksPageState {
  nodes: NodeMap;
  selectedFolder: string;
  folderOpenState: FolderOpenState;
  prefs: PreferencesState;
  search: SearchState;
  selection: SelectionState;
}

export interface DropDestination {
  element: BookmarkElement;
  position: DropPosition;
}

export class BookmarkElement extends HTMLElement {
  itemId: string = '';

  getDropTarget(): HTMLElement|null {
    return null;
  }
}

export class DragData {
  elements: chrome.bookmarks.BookmarkTreeNode[]|null = null;
  sameProfile: boolean = false;
}

export type TimerProxy = Pick<Window, 'setTimeout'|'clearTimeout'>;
