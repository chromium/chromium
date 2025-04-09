// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {ACCOUNT_HEADING_NODE_ID, BOOKMARKS_BAR_ID, IncognitoAvailability, LOCAL_HEADING_NODE_ID, ROOT_NODE_ID} from './constants.js';
import type {BookmarkNode, BookmarksPageState, NodeMap, ObjectMap} from './types.js';

/**
 * @fileoverview Utility functions for the Bookmarks page.
 */

export function getDisplayedList(state: BookmarksPageState): string[] {
  if (isShowingSearch(state)) {
    assert(state.search.results);
    return state.search.results;
  }

  const selectedNode = state.nodes[state.selectedFolder];
  assert(selectedNode);
  const children = selectedNode.children;
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

function hasBothLocalAndAccountBookmarksBar(
    nodes: chrome.bookmarks.BookmarkTreeNode[]): boolean {
  return nodes.some(
             child => child.folderType ===
                     chrome.bookmarks.FolderType.BOOKMARKS_BAR &&
                 child.syncing) &&
      nodes.some(
          child =>
              child.folderType === chrome.bookmarks.FolderType.BOOKMARKS_BAR &&
              !child.syncing);
}

function buildAccountHeadingNode(): BookmarkNode {
  return {
    id: ACCOUNT_HEADING_NODE_ID,
    title: loadTimeData.getString('accountBookmarksTitle'),
    parentId: ROOT_NODE_ID,
    children: [],
  };
}

function buildLocalHeadingNode(): BookmarkNode {
  return {
    id: LOCAL_HEADING_NODE_ID,
    title: loadTimeData.getString('localBookmarksTitle'),
    parentId: ROOT_NODE_ID,
    children: [],
  };
}

export function normalizeNodes(rootNode: chrome.bookmarks.BookmarkTreeNode):
    NodeMap {
  const nodeMap: NodeMap = {};
  const stack = [];
  stack.push(rootNode);

  // If the user has both local and account bookmarks bars, insert heading nodes
  // to distinguish them.
  const addHeadingNodes =
      hasBothLocalAndAccountBookmarksBar(rootNode.children!);

  while (stack.length > 0) {
    const node = stack.pop()!;
    nodeMap[node.id] = normalizeNode(node);

    if (node.children) {
      node.children.forEach(function(child) {
        stack.push(child);
      });
    }

    if (addHeadingNodes) {
      if (node.id === rootNode.id) {
        // Clear the children set on the root node, and add the heading nodes as
        // children.
        nodeMap[node.id]!.children = [];
        for (const headingNode
                 of [buildAccountHeadingNode(), buildLocalHeadingNode()]) {
          nodeMap[headingNode.id] = headingNode;
          nodeMap[node.id]!.children!.push(headingNode.id);
        }
      } else if (node.parentId === rootNode.id) {
        // Replace the parent with the appropriate heading nodes.
        const headingNode = node.syncing ? nodeMap[ACCOUNT_HEADING_NODE_ID]! :
                                           nodeMap[LOCAL_HEADING_NODE_ID]!;
        nodeMap[node.id]!.parentId = headingNode.id;
        headingNode.children!.unshift(node.id);
      }
    }
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
  return !isRootOrChildOfRoot(state, itemId) && !!state.nodes[itemId] &&
      !state.nodes[itemId].unmodifiable && state.prefs.canEdit;
}

/**
 * Returns true if it is possible to modify the children list of the node with
 * ID |itemId|. This includes rearranging the children or adding new ones.
 */
export function canReorderChildren(
    state: BookmarksPageState, itemId: string): boolean {
  return !isRootNode(itemId) && !!state.nodes[itemId] &&
      !state.nodes[itemId].unmodifiable && state.prefs.canEdit;
}

export function hasChildFolders(id: string, nodes: NodeMap): boolean {
  if (!nodes[id] || !nodes[id].children) {
    return false;
  }

  const children = nodes[id].children;
  for (let i = 0; i < children.length; i++) {
    if (nodes[children[i]!]?.children) {
      return true;
    }
  }
  return false;
}

export function getDescendants(nodes: NodeMap, baseId: string): Set<string> {
  const descendants = new Set<string>();
  const stack: string[] = [];
  stack.push(baseId);

  while (stack.length > 0) {
    const id = stack.pop()!;
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

// Whether this is either the root node, or one of the account/local heading
// nodes.
export function isRootNode(itemId: string): boolean {
  const rootNodesIds =
      new Set([ROOT_NODE_ID, ACCOUNT_HEADING_NODE_ID, LOCAL_HEADING_NODE_ID]);
  return rootNodesIds.has(itemId);
}

/**
 * Whether the node with ID `itemId` satisfies `isRootNode()`, or its parent
 * satisfies `isRootNode()`.
 */
export function isRootOrChildOfRoot(
    state: BookmarksPageState, itemId: string): boolean {
  if (isRootNode(itemId)) {
    return true;
  }

  const node = state.nodes[itemId];
  if (!node) {
    return false;
  }

  assert(node.parentId);
  return isRootNode(node.parentId);
}
