// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {PermanentFolderType} from './bookmarks_api.mojom-webui.js';
import type {BookmarkNode as MojoBookmarkNode, Folder as MojoFolder, RootNode as MojoRootNode} from './bookmarks_api.mojom-webui.js';
import {ACCOUNT_HEADING_NODE_ID, IncognitoAvailability, LOCAL_HEADING_NODE_ID, ROOT_NODE_ID} from './constants.js';
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

export function getDefaultSelectedFolder(nodes: NodeMap): string {
  const selectedFolderParent =
      nodes[ACCOUNT_HEADING_NODE_ID] || nodes[ROOT_NODE_ID];
  assert(selectedFolderParent);
  assert(selectedFolderParent.children);

  let selectedFolder = '';
  // Select the account bookmarks bar if it exists. If not, use the local
  // bookmarks bar.
  for (const id of selectedFolderParent.children) {
    const node = nodes[id];
    assert(node);
    if (node.permanentFolderType === PermanentFolderType.kBookmarkBar) {
      selectedFolder = id;
      if (node.isSynced) {
        // If this is the account bookmarks bar, stop immediately so it
        // takes precedence over the local counterpart.
        break;
      }
    }
  }
  assert(selectedFolder);
  return selectedFolder;
}

export function normalizeMojoRootNode(mojoRoot: MojoRootNode): BookmarkNode {
  return {
    id: ROOT_NODE_ID,
    title: '',
    isSynced: false,
    children: mojoRoot.children.map(child => child.id!.value),
  };
}

export function normalizeMojoNode(
    mojoNode: MojoBookmarkNode, parentId: string): BookmarkNode {
  if (mojoNode.url) {
    const urlNode = mojoNode.url;
    const result: BookmarkNode = {
      id: urlNode.id!.value,
      title: urlNode.title,
      url: urlNode.url,
      parentId: parentId,
      isSynced: urlNode.isSynced,
    };
    if (urlNode.legacy?.id !== undefined && urlNode.legacy?.id !== null) {
      result.legacyId = Number(urlNode.legacy.id);
    }
    return result;
  }

  if (mojoNode.folder) {
    const folderNode = mojoNode.folder;
    const result: BookmarkNode = {
      id: folderNode.id?.value || '',
      title: folderNode.title,
      children: folderNode.children.map(child => {
        return child.url?.id?.value || child.folder?.id?.value || '';
      }),
      parentId: parentId,
      isSynced: folderNode.isSynced,
    };
    if (folderNode.permanentFolderType !== null &&
        folderNode.permanentFolderType !== undefined &&
        folderNode.permanentFolderType !== PermanentFolderType.kUnknown) {
      result.permanentFolderType = folderNode.permanentFolderType;
    }
    if (folderNode.legacy?.id !== undefined && folderNode.legacy?.id !== null) {
      result.legacyId = Number(folderNode.legacy.id);
    }
    return result;
  }

  assertNotReached();
}

/**
 * Converts a storage BookmarkNode to a MojoBookmarkNode.
 * Inverse of normalizeMojoNode.
 */
export function toMojoNode(node: BookmarkNode): MojoBookmarkNode {
  const id = node.id ? {value: node.id} : null;
  const title = node.title;
  const isSynced = node.isSynced ?? false;
  const legacy =
      node.legacyId !== undefined ? {id: BigInt(node.legacyId)} : null;

  if (node.url !== undefined) {
    return {
      url: {
        id: id,
        title: title,
        url: node.url,
        faviconUrl: null,
        isSynced: isSynced,
        legacy: legacy,
      },
    };
  }

  return {
    folder: {
      id: id,
      title: title,
      children: [],
      permanentFolderType: node.permanentFolderType ?? null,
      isSynced: isSynced,
      legacy: legacy,
    },
  };
}

function hasBothLocalAndAccountBookmarksBar(nodes: MojoFolder[]): boolean {
  return nodes.some(
             child => child.permanentFolderType ===
                     PermanentFolderType.kBookmarkBar &&
                 child.isSynced) &&
      nodes.some(
          child =>
              child.permanentFolderType === PermanentFolderType.kBookmarkBar &&
              !child.isSynced);
}

export function buildAccountHeadingNode(): BookmarkNode {
  return {
    id: ACCOUNT_HEADING_NODE_ID,
    title: loadTimeData.getString('accountBookmarksTitle'),
    parentId: ROOT_NODE_ID,
    children: [],
  };
}

export function buildLocalHeadingNode(): BookmarkNode {
  return {
    id: LOCAL_HEADING_NODE_ID,
    title: loadTimeData.getString('localBookmarksTitle'),
    parentId: ROOT_NODE_ID,
    children: [],
  };
}

export function normalizeNodes(rootNode: MojoRootNode): NodeMap {
  const nodeMap: NodeMap = {};
  const stack: Array<{node: MojoBookmarkNode, parentId: string}> = [];

  const normalizedRoot = normalizeMojoRootNode(rootNode);
  nodeMap[normalizedRoot.id] = normalizedRoot;

  const addHeadingNodes = hasBothLocalAndAccountBookmarksBar(rootNode.children);

  if (addHeadingNodes) {
    normalizedRoot.children = [];
    for (const headingNode
             of [buildAccountHeadingNode(), buildLocalHeadingNode()]) {
      nodeMap[headingNode.id] = headingNode;
      normalizedRoot.children.push(headingNode.id);
    }
  }

  rootNode.children.forEach(child => {
    let parentId = normalizedRoot.id;
    if (addHeadingNodes) {
      parentId =
          child.isSynced ? ACCOUNT_HEADING_NODE_ID : LOCAL_HEADING_NODE_ID;
      nodeMap[parentId]!.children!.push(child.id?.value || '');
    }
    stack.push({node: {folder: child}, parentId: parentId});
  });

  while (stack.length > 0) {
    const {node, parentId} = stack.pop()!;
    const normalized = normalizeMojoNode(node, parentId);
    nodeMap[normalized.id] = normalized;

    const folder = node.folder;
    if (folder && folder.children) {
      folder.children.forEach(child => {
        stack.push({node: child, parentId: normalized.id});
      });
    }
  }

  return nodeMap;
}

export function createEmptyState(): BookmarksPageState {
  return {
    nodes: {},
    selectedFolder: '',
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

function isManaged(nodes: NodeMap, itemId: string): boolean {
  let id = itemId;
  while (id) {
    const node = nodes[id];
    if (!node) {
      break;
    }
    if (node.permanentFolderType === PermanentFolderType.kManaged) {
      return true;
    }
    id = node.parentId || '';
  }
  return false;
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
      !isManaged(state.nodes, itemId) && state.prefs.canEdit;
}

/**
 * Returns true if it is possible to modify the children list of the node with
 * ID |itemId|. This includes rearranging the children or adding new ones.
 */
export function canReorderChildren(
    state: BookmarksPageState, itemId: string): boolean {
  return !isRootNode(itemId) && !!state.nodes[itemId] &&
      !isManaged(state.nodes, itemId) && state.prefs.canEdit;
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

/**
 * Flattens the bookmark hierarchy into an ordered array of BookmarkNodes
 * in hierarchical (pre-order depth-first) order. Root and permanent folder
 * container nodes are excluded from the result list.
 */
export function flattenNodes(
    nodes: NodeMap, baseId: string = ROOT_NODE_ID): BookmarkNode[] {
  const flattened: BookmarkNode[] = [];

  function traverse(id: string) {
    const node = nodes[id];
    if (!node) {
      return;
    }

    if (!isRootNode(node.id) && node.permanentFolderType === undefined) {
      flattened.push(node);
    }

    if (node.children) {
      for (const childId of node.children) {
        traverse(childId);
      }
    }
  }

  traverse(baseId);
  return flattened;
}

export function searchBookmarks(nodes: NodeMap, term: string): BookmarkNode[] {
  const tokens = term.toLowerCase().split(/\s+/).filter(t => t.length > 0);
  if (tokens.length === 0) {
    return [];
  }

  return flattenNodes(nodes).filter(node => {
    const title = node.title.toLowerCase();
    const url = node.url ? node.url.toLowerCase() : '';
    return tokens.every(
        token => title.includes(token) || (!!url && url.includes(token)));
  });
}

export function getLegacyId(node: BookmarkNode|undefined): string {
  // This is mainly to work around the annoying redux store lookup, which might
  // yield undefined for an id. Once we move off of this, we don't need this
  // shim anymore.
  assert(!!node && (!!node.legacyId || node.legacyId === 0));
  return node.legacyId.toString();
}
