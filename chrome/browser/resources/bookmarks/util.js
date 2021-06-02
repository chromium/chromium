// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {BOOKMARKS_BAR_ID, IncognitoAvailability, ROOT_NODE_ID} from './constants.js';
import {BookmarkNode, BookmarksPageState, NodeMap} from './types.js';

/**
 * @fileoverview Utility functions for the Bookmarks page.
 */

/**
 * Returns the list of bookmark IDs to be displayed in the UI, taking into
 * account search and the currently selected folder.
 * @param {!BookmarksPageState} state
 * @return {!Array<string>}
 */
export function getDisplayedList(state) {
  if (isShowingSearch(state)) {
    return assert(state.search.results);
  }

  return assert(state.nodes[state.selectedFolder].children);
}

/**
 * @param {chrome.bookmarks.BookmarkTreeNode} treeNode
 * @return {!BookmarkNode}
 */
export function normalizeNode(treeNode) {
  const node = Object.assign({}, treeNode);
  // Node index is not necessary and not kept up-to-date. Remove it from the
  // data structure so we don't accidentally depend on the incorrect
  // information.
  delete node.index;

  if (!('url' in node)) {
    // The onCreated API listener returns folders without |children| defined.
    node.children = (node.children || []).map(function(child) {
      return child.id;
    });
  }
  return /** @type {BookmarkNode} */ (node);
}

/**
 * @param {chrome.bookmarks.BookmarkTreeNode} rootNode
 * @return {NodeMap}
 */
export function normalizeNodes(rootNode) {
  /** @type {NodeMap} */
  const nodeMap = {};
  const stack = [];
  stack.push(rootNode);

  while (stack.length > 0) {
    const node = stack.pop();
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

/** @return {!BookmarksPageState} */
export function createEmptyState() {
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

/**
 * @param {BookmarksPageState} state
 * @return {boolean}
 */
export function isShowingSearch(state) {
  return state.search.results != null;
}

/**
 * Returns true if the node with ID |itemId| is modifiable, allowing
 * the node to be renamed, moved or deleted. Note that if a node is
 * uneditable, it may still have editable children (for example, the top-level
 * folders).
 * @param {BookmarksPageState} state
 * @param {string} itemId
 * @return {boolean}
 */
export function canEditNode(state, itemId) {
  return itemId !== ROOT_NODE_ID &&
      state.nodes[itemId].parentId !== ROOT_NODE_ID &&
      !state.nodes[itemId].unmodifiable && state.prefs.canEdit;
}

/**
 * Returns true if it is possible to modify the children list of the node with
 * ID |itemId|. This includes rearranging the children or adding new ones.
 * @param {BookmarksPageState} state
 * @param {string} itemId
 * @return {boolean}
 */
export function canReorderChildren(state, itemId) {
  return itemId !== ROOT_NODE_ID && !state.nodes[itemId].unmodifiable &&
      state.prefs.canEdit;
}

/**
 * @param {string} id
 * @param {NodeMap} nodes
 * @return {boolean}
 */
export function hasChildFolders(id, nodes) {
  const children = nodes[id].children;
  for (let i = 0; i < children.length; i++) {
    if (nodes[children[i]].children) {
      return true;
    }
  }
  return false;
}

/**
 * Get all descendants of a node, including the node itself.
 * @param {NodeMap} nodes
 * @param {string} baseId
 * @return {!Set<string>}
 */
export function getDescendants(nodes, baseId) {
  const descendants = new Set();
  const stack = [];
  stack.push(baseId);

  while (stack.length > 0) {
    const id = stack.pop();
    const node = nodes[id];

    if (!node) {
      continue;
    }

    descendants.add(id);

    if (!node.children) {
      continue;
    }

    node.children.forEach(function(childId) {
      stack.push(childId);
    });
  }

  return descendants;
}

/**
 * @param {!Object<string, T>} map
 * @param {!Set<string>} ids
 * @return {!Object<string, T>}
 * @template T
 */
export function removeIdsFromObject(map, ids) {
  const newObject = Object.assign({}, map);
  ids.forEach(function(id) {
    delete newObject[id];
  });
  return newObject;
}


/**
 * @param {!Map<string, T>} map
 * @param {!Set<string>} ids
 * @return {!Map<string, T>}
 * @template T
 */
export function removeIdsFromMap(map, ids) {
  const newMap = new Map(map);
  ids.forEach(function(id) {
    newMap.delete(id);
  });
  return newMap;
}

/**
 * @param {!Set<string>} set
 * @param {!Set<string>} ids
 * @return {!Set<string>}
 */
export function removeIdsFromSet(set, ids) {
  const difference = new Set(set);
  ids.forEach(function(id) {
    difference.delete(id);
  });
  return difference;
}
