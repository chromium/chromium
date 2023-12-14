// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {BOOKMARKS_BAR_ID, IncognitoAvailability, ROOT_NODE_ID} from './constants.js';
import type {BookmarkNode, BookmarksPageState, NodeMap, ObjectMap} from './types.js';

/**
 * @fileoverview Utility functions for the Bookmarks page.
 */

export function getDisplayedList(state: BookmarksPageState): string[] {
  if (isShowingSearch(state)) {
    assert(state.search.results);
    return state.search.results;
  }

  const children = state.nodes[state.selectedFolder]!.children;
  assert(children);
  return children;
}

export function normalizeNode(treeNode: chrome.bookmarks.BookmarkTreeNode):
    BookmarkNode {
  const node = Object.assign({}, treeNode);
  // Node index is not necessary and not kept up-to-date. Remove it from the
  // data structure so we don't accidentally depend on the incorrect
  // information.
  delete node.index;
  delete node.children;
  const bookmarkNode = node as unknown as BookmarkNode;

  if (!('url' in node)) {
    // The onCreated API listener returns folders without |children| defined.
    bookmarkNode.children = (treeNode.children || []).map(function(child) {
      return child.id;
    });
  }
  return bookmarkNode;
}

export function normalizeNodes(rootNode: chrome.bookmarks.BookmarkTreeNode):
    NodeMap {
  const nodeMap: NodeMap = {};
  const stack = [];
  stack.push(rootNode);

  while (stack.length > 0) {
    const node = stack.pop()!;
    nodeMap[node.id] = normalizeNode(node);
    if (!node.children) {
      continue;
    }

    node.children.forEach(function(child) {
      stack.push(child);
    });
  }

  return nodeMap;
}

export function createEmptyState(): BookmarksPageState {
  return {
    nodes: {},
    selectedFolder: BOOKMARKS_BAR_ID,
    folderOpenState: new Map(),
    prefs: {
      canEdit: true,
      incognitoAvailability: IncognitoAvailability.ENABLED,
    },
    search: {
      term: '',
      inProgress: false,
      results: null,
    },
    selection: {
      items: new Set(),
      anchor: null,
    },
  };
}

export function isShowingSearch(state: BookmarksPageState): boolean {
  return state.search.results != null;
}

/**
 * Returns true if the node with ID |itemId| is modifiable, allowing
 * the node to be renamed, moved or deleted. Note that if a node is
 * uneditable, it may still have editable children (for example, the top-level
 * folders).
 */
export function canEditNode(
    state: BookmarksPageState, itemId: string): boolean {
  return itemId !== ROOT_NODE_ID &&
      state.nodes![itemId]!.parentId !== ROOT_NODE_ID &&
      !state.nodes![itemId]!.unmodifiable && state.prefs.canEdit;
}

/**
 * Returns true if it is possible to modify the children list of the node with
 * ID |itemId|. This includes rearranging the children or adding new ones.
 */
export function canReorderChildren(
    state: BookmarksPageState, itemId: string): boolean {
  return itemId !== ROOT_NODE_ID && !state.nodes[itemId]!.unmodifiable &&
      state.prefs.canEdit;
}

export function hasChildFolders(id: string, nodes: NodeMap): boolean {
  const children = nodes[id]!.children!;
  for (let i = 0; i < children.length; i++) {
    if (nodes[children[i]!]!.children) {
      return true;
    }
  }
  return false;
}

export function getDescendants(nodes: NodeMap, baseId: string): Set<string> {
  const descendants = new Set() as Set<string>;
  const stack: string[] = [];
  stack.push(baseId);

  while (stack.length > 0) {
    const id = stack.pop()!;
    const node = nodes[id];

    if (!node) {
      continue;
    }

    descendants.add(id);

    if (!node!.children) {
      continue;
    }

    node!.children.forEach(function(childId) {
      stack.push(childId);
    });
  }

  return descendants;
}

export function removeIdsFromObject<Type>(
    map: ObjectMap<Type>, ids: Set<string>): ObjectMap<Type> {
  const newObject = Object.assign({}, map);
  ids.forEach(function(id) {
    delete newObject[id];
  });
  return newObject;
}


export function removeIdsFromMap<Type>(
    map: Map<string, Type>, ids: Set<string>): Map<string, Type> {
  const newMap = new Map(map);
  ids.forEach(function(id) {
    newMap.delete(id);
  });
  return newMap;
}

export function removeIdsFromSet(
    set: Set<string>, ids: Set<string>): Set<string> {
  const difference = new Set(set);
  ids.forEach(function(id) {
    difference.delete(id);
  });
  return difference;
}
