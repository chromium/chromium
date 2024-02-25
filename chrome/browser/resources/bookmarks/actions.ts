// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import type {Action} from 'chrome://resources/js/store.js';

import {IncognitoAvailability, ROOT_NODE_ID} from './constants.js';
import type {BookmarkNode, BookmarksPageState, NodeMap} from './types.js';
import {getDescendants, getDisplayedList, normalizeNode} from './util.js';

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

export type CreateBookmarkAction = Action&{
  id: string,
  parentId: string,
  parentIndex: number,
  node: BookmarkNode,
};

export function createBookmark(
    id: string,
    treeNode: chrome.bookmarks.BookmarkTreeNode): CreateBookmarkAction {
  return {
    name: 'create-bookmark',
    id: id,
    parentId: treeNode.parentId!,
    parentIndex: treeNode.index!,
    node: normalizeNode(treeNode),
  };
}

export type EditBookmarkAction = Action&{
  id: string,
  changeInfo: {title: string, url?: string},
};

export function editBookmark(
    id: string, changeInfo: {title: string, url?: string}): EditBookmarkAction {
  return {
    name: 'edit-bookmark',
    id: id,
    changeInfo: changeInfo,
  };
}

export type MoveBookmarkAction = Action&{
  id: string,
  parentId: string,
  index: number,
  oldParentId: string,
  oldIndex: number,
};

export function moveBookmark(
    id: string, parentId: string, index: number, oldParentId: string,
    oldIndex: number): MoveBookmarkAction {
  return {
    name: 'move-bookmark',
    id: id,
    parentId: parentId,
    index: index,
    oldParentId: oldParentId,
    oldIndex: oldIndex,
  };
}

export type ReorderChildrenAction = Action&{
  id: string,
  children: string[],
};

export function reorderChildren(
    id: string, newChildIds: string[]): ReorderChildrenAction {
  return {
    name: 'reorder-children',
    id: id,
    children: newChildIds,
  };
}

export type RemoveBookmarkAction = Action&{
  id: string,
  parentId: string,
  index: number,
  descendants: Set<string>,
};

export function removeBookmark(
    id: string, parentId: string, index: number,
    nodes: NodeMap): RemoveBookmarkAction {
  const descendants = getDescendants(nodes, id);
  return {
    name: 'remove-bookmark',
    id: id,
    descendants: descendants,
    parentId: parentId,
    index: index,
  };
}

export type RefreshNodesAction = Action&{
  nodes: NodeMap,
};

export function refreshNodes(nodeMap: NodeMap): RefreshNodesAction {
  return {
    name: 'refresh-nodes',
    nodes: nodeMap,
  };
}

export type SelectFolderAction = Action&{
  id: string,
};

export function selectFolder(id: string, nodes?: NodeMap): SelectFolderAction|
    null {
  if (nodes && (id === ROOT_NODE_ID || !nodes[id] || nodes[id]!.url)) {
    console.warn('Tried to select invalid folder: ' + id);
    return null;
  }

  return {
    name: 'select-folder',
    id: id,
  };
}

export type ChangeFolderOpenAction = Action&{
  id: string,
  open: boolean,
};

export function changeFolderOpen(
    id: string, open: boolean): ChangeFolderOpenAction {
  return {
    name: 'change-folder-open',
    id: id,
    open: open,
  };
}

export function clearSearch(): Action {
  return {
    name: 'clear-search',
  };
}

export function deselectItems(): Action {
  return {
    name: 'deselect-items',
  };
}

export type SelectItemsAction = Action&{
  clear: boolean,
  toggle: boolean,
  anchor: string,
  items: string[],
};

export function selectItem(
    id: string, state: BookmarksPageState,
    config: {clear: boolean, range: boolean, toggle: boolean}):
    SelectItemsAction {
  assert(!config.toggle || !config.range);
  assert(!config.toggle || !config.clear);

  const anchor = state.selection.anchor;
  const toSelect: string[] = [];
  let newAnchor = id;

  if (config.range && anchor) {
    const displayedList = getDisplayedList(state);
    const selectedIndex = displayedList.indexOf(id);
    assert(selectedIndex !== -1);
    let anchorIndex = displayedList.indexOf(anchor);
    if (anchorIndex === -1) {
      anchorIndex = selectedIndex;
    }

    // When performing a range selection, don't change the anchor from what
    // was used in this selection.
    newAnchor = displayedList[anchorIndex]!;

    const startIndex = Math.min(anchorIndex, selectedIndex);
    const endIndex = Math.max(anchorIndex, selectedIndex);

    for (let i = startIndex; i <= endIndex; i++) {
      toSelect.push(displayedList[i]!);
    }
  } else {
    toSelect.push(id);
  }

  return {
    name: 'select-items',
    clear: config.clear,
    toggle: config.toggle,
    anchor: newAnchor,
    items: toSelect,
  };
}

export function selectAll(
    ids: string[], state: BookmarksPageState,
    anchor?: string): SelectItemsAction {
  const finalAnchor: string = anchor ? anchor! : state.selection!.anchor!;
  return {
    name: 'select-items',
    clear: true,
    toggle: false,
    anchor: finalAnchor,
    items: ids,
  };
}

export type UpdateAnchorAction = Action&{
  anchor: string,
};

export function updateAnchor(id: string): UpdateAnchorAction {
  return {
    name: 'update-anchor',
    anchor: id,
  };
}

export type StartSearchAction = Action&{
  term: string,
};

export function setSearchTerm(term: string): (Action|StartSearchAction) {
  if (!term) {
    return clearSearch();
  }

  return {
    name: 'start-search',
    term: term,
  };
}

export type FinishSearchAction = Action&{
  results: string[],
};

export function setSearchResults(ids: string[]): FinishSearchAction {
  return {
    name: 'finish-search',
    results: ids,
  };
}

export type SetPrefAction = Action&{
  value: IncognitoAvailability | boolean,
};

export function setIncognitoAvailability(availability: IncognitoAvailability):
    SetPrefAction {
  assert(availability !== IncognitoAvailability.FORCED);
  return {
    name: 'set-incognito-availability',
    value: availability,
  };
}

export function setCanEditBookmarks(canEdit: boolean): SetPrefAction {
  return {
    name: 'set-can-edit',
    value: canEdit,
  };
}
