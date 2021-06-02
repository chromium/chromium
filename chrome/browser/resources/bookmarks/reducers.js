// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {BookmarkNode, BookmarksPageState, FolderOpenState, NodeMap, PreferencesState, SearchState, SelectionState} from './types.js';
import {removeIdsFromMap, removeIdsFromObject, removeIdsFromSet} from './util.js';

/**
 * @fileoverview Module of functions which produce a new page state in response
 * to an action. Reducers (in the same sense as Array.prototype.reduce) must be
 * pure functions: they must not modify existing state objects, or make any API
 * calls.
 */

/**
 * @param {SelectionState} selectionState
 * @param {Object} action
 * @return {SelectionState}
 */
function selectItems(selectionState, action) {
  let newItems = new Set();
  if (!action.clear) {
    newItems = new Set(selectionState.items);
  }

  action.items.forEach(function(id) {
    let add = true;
    if (action.toggle) {
      add = !newItems.has(id);
    }

    if (add) {
      newItems.add(id);
    } else {
      newItems.delete(id);
    }
  });

  return /** @type {SelectionState} */ (Object.assign({}, selectionState, {
    items: newItems,
    anchor: action.anchor,
  }));
}

/**
 * @param {SelectionState} selectionState
 * @return {SelectionState}
 */
function deselectAll(selectionState) {
  return {
    items: new Set(),
    anchor: null,
  };
}

/**
 * @param {SelectionState} selectionState
 * @param {!Set<string>} deleted
 * @return SelectionState
 */
function deselectItems(selectionState, deleted) {
  return /** @type {SelectionState} */ (Object.assign({}, selectionState, {
    items: removeIdsFromSet(selectionState.items, deleted),
    anchor: !selectionState.anchor || deleted.has(selectionState.anchor) ?
        null :
        selectionState.anchor,
  }));
}

/**
 * @param {SelectionState} selectionState
 * @param {Object} action
 * @return {SelectionState}
 */
function updateAnchor(selectionState, action) {
  return /** @type {SelectionState} */ (Object.assign({}, selectionState, {
    anchor: action.anchor,
  }));
}

/**
 * Exported for tests.
 * @param {SelectionState} selection
 * @param {Object} action
 * @return {SelectionState}
 */
export function updateSelection(selection, action) {
  switch (action.name) {
    case 'clear-search':
    case 'finish-search':
    case 'select-folder':
    case 'deselect-items':
      return deselectAll(selection);
    case 'select-items':
      return selectItems(selection, action);
    case 'remove-bookmark':
      return deselectItems(selection, action.descendants);
    case 'move-bookmark':
      // Deselect items when they are moved to another folder, since they will
      // no longer be visible on screen (for simplicity, ignores items visible
      // in search results).
      if (action.parentId !== action.oldParentId &&
          selection.items.has(action.id)) {
        return deselectItems(selection, new Set([action.id]));
      }
      return selection;
    case 'update-anchor':
      return updateAnchor(selection, action);
    default:
      return selection;
  }
}

/**
 * @param {SearchState} search
 * @param {Object} action
 * @return {SearchState}
 */
function startSearch(search, action) {
  return {
    term: action.term,
    inProgress: true,
    results: search.results,
  };
}

/**
 * @param {SearchState} search
 * @param {Object} action
 * @return {SearchState}
 */
function finishSearch(search, action) {
  return /** @type {SearchState} */ (Object.assign({}, search, {
    inProgress: false,
    results: action.results,
  }));
}

/** @return {SearchState} */
function clearSearch() {
  return {
    term: '',
    inProgress: false,
    results: null,
  };
}

/**
 * @param {SearchState} search
 * @param {!Set<string>} deletedIds
 * @return {SearchState}
 */
function removeDeletedResults(search, deletedIds) {
  if (!search.results) {
    return search;
  }

  const newResults = [];
  search.results.forEach(function(id) {
    if (!deletedIds.has(id)) {
      newResults.push(id);
    }
  });
  return /** @type {SearchState} */ (Object.assign({}, search, {
    results: newResults,
  }));
}

/**
 * @param {SearchState} search
 * @param {Object} action
 * @return {SearchState}
 */
function updateSearch(search, action) {
  switch (action.name) {
    case 'start-search':
      return startSearch(search, action);
    case 'select-folder':
    case 'clear-search':
      return clearSearch();
    case 'finish-search':
      return finishSearch(search, action);
    case 'remove-bookmark':
      return removeDeletedResults(search, action.descendants);
    default:
      return search;
  }
}

/**
 * @param {NodeMap} nodes
 * @param {string} id
 * @param {function(BookmarkNode):BookmarkNode} callback
 * @return {NodeMap}
 */
function modifyNode(nodes, id, callback) {
  const nodeModification = {};
  nodeModification[id] = callback(nodes[id]);
  return Object.assign({}, nodes, nodeModification);
}

/**
 * @param {NodeMap} nodes
 * @param {Object} action
 * @return {NodeMap}
 */
function createBookmark(nodes, action) {
  const nodeModifications = {};
  nodeModifications[action.id] = action.node;

  const parentNode = nodes[action.parentId];
  const newChildren = parentNode.children.slice();
  newChildren.splice(action.parentIndex, 0, action.id);
  nodeModifications[action.parentId] = Object.assign({}, parentNode, {
    children: newChildren,
  });

  return Object.assign({}, nodes, nodeModifications);
}

/**
 * @param {NodeMap} nodes
 * @param {Object} action
 * @return {NodeMap}
 */
function editBookmark(nodes, action) {
  // Do not allow folders to change URL (making them no longer folders).
  if (!nodes[action.id].url && action.changeInfo.url) {
    delete action.changeInfo.url;
  }

  return modifyNode(nodes, action.id, function(node) {
    return /** @type {BookmarkNode} */ (
        Object.assign({}, node, action.changeInfo));
  });
}

/**
 * @param {NodeMap} nodes
 * @param {Object} action
 * @return {NodeMap}
 */
function moveBookmark(nodes, action) {
  const nodeModifications = {};
  const id = action.id;

  // Change node's parent.
  nodeModifications[id] =
      Object.assign({}, nodes[id], {parentId: action.parentId});

  // Remove from old parent.
  const oldParentId = action.oldParentId;
  const oldParentChildren = nodes[oldParentId].children.slice();
  oldParentChildren.splice(action.oldIndex, 1);
  nodeModifications[oldParentId] =
      Object.assign({}, nodes[oldParentId], {children: oldParentChildren});

  // Add to new parent.
  const parentId = action.parentId;
  const parentChildren = oldParentId === parentId ?
      oldParentChildren :
      nodes[parentId].children.slice();
  parentChildren.splice(action.index, 0, action.id);
  nodeModifications[parentId] =
      Object.assign({}, nodes[parentId], {children: parentChildren});

  return Object.assign({}, nodes, nodeModifications);
}

/**
 * @param {NodeMap} nodes
 * @param {Object} action
 * @return {NodeMap}
 */
function removeBookmark(nodes, action) {
  const newState = modifyNode(nodes, action.parentId, function(node) {
    const newChildren = node.children.slice();
    newChildren.splice(action.index, 1);
    return /** @type {BookmarkNode} */ (
        Object.assign({}, node, {children: newChildren}));
  });

  return removeIdsFromObject(newState, action.descendants);
}

/**
 * @param {NodeMap} nodes
 * @param {Object} action
 * @return {NodeMap}
 */
function reorderChildren(nodes, action) {
  return modifyNode(nodes, action.id, function(node) {
    return /** @type {BookmarkNode} */ (
        Object.assign({}, node, {children: action.children}));
  });
}

/**
 * Exported for tests.
 * @param {NodeMap} nodes
 * @param {Object} action
 * @return {NodeMap}
 */
export function updateNodes(nodes, action) {
  switch (action.name) {
    case 'create-bookmark':
      return createBookmark(nodes, action);
    case 'edit-bookmark':
      return editBookmark(nodes, action);
    case 'move-bookmark':
      return moveBookmark(nodes, action);
    case 'remove-bookmark':
      return removeBookmark(nodes, action);
    case 'reorder-children':
      return reorderChildren(nodes, action);
    case 'refresh-nodes':
      return action.nodes;
    default:
      return nodes;
  }
}

/**
 * @param {NodeMap} nodes
 * @param {string} ancestorId
 * @param {string} childId
 * @return {boolean}
 */
function isAncestorOf(nodes, ancestorId, childId) {
  let currentId = childId;
  // Work upwards through the tree from child.
  while (currentId) {
    if (currentId === ancestorId) {
      return true;
    }
    currentId = nodes[currentId].parentId;
  }
  return false;
}

/**
 * Exported for tests.
 * @param {string} selectedFolder
 * @param {Object} action
 * @param {NodeMap} nodes
 * @return {string}
 */
export function updateSelectedFolder(selectedFolder, action, nodes) {
  switch (action.name) {
    case 'select-folder':
      return action.id;
    case 'change-folder-open':
      // When hiding the selected folder by closing its ancestor, select
      // that ancestor instead.
      if (!action.open && selectedFolder &&
          isAncestorOf(nodes, action.id, selectedFolder)) {
        return action.id;
      }
      return selectedFolder;
    case 'remove-bookmark':
      // When deleting the selected folder (or its ancestor), select the
      // parent of the deleted node.
      if (selectedFolder && isAncestorOf(nodes, action.id, selectedFolder)) {
        return assert(nodes[action.id].parentId);
      }
      return selectedFolder;
    default:
      return selectedFolder;
  }
}

/**
 * @param {FolderOpenState} folderOpenState
 * @param {string|undefined} id
 * @param {NodeMap} nodes
 * @return {FolderOpenState}
 */
function openFolderAndAncestors(folderOpenState, id, nodes) {
  const newFolderOpenState =
      /** @type {FolderOpenState} */ (new Map(folderOpenState));
  for (let currentId = id; currentId; currentId = nodes[currentId].parentId) {
    newFolderOpenState.set(currentId, true);
  }

  return newFolderOpenState;
}

/**
 * @param {FolderOpenState} folderOpenState
 * @param {Object} action
 * @return {FolderOpenState}
 */
function changeFolderOpen(folderOpenState, action) {
  const newFolderOpenState =
      /** @type {FolderOpenState} */ (new Map(folderOpenState));
  newFolderOpenState.set(action.id, action.open);

  return newFolderOpenState;
}

/**
 * Exported for tests.
 * @param {FolderOpenState} folderOpenState
 * @param {Object} action
 * @param {NodeMap} nodes
 * @return {FolderOpenState}
 */
export function updateFolderOpenState(folderOpenState, action, nodes) {
  switch (action.name) {
    case 'change-folder-open':
      return changeFolderOpen(folderOpenState, action);
    case 'select-folder':
      return openFolderAndAncestors(
          folderOpenState, nodes[action.id].parentId, nodes);
    case 'move-bookmark':
      if (!nodes[action.id].children) {
        return folderOpenState;
      }

      return openFolderAndAncestors(folderOpenState, action.parentId, nodes);
    case 'remove-bookmark':
      return removeIdsFromMap(folderOpenState, action.descendants);
    default:
      return folderOpenState;
  }
}

/**
 * @param {PreferencesState} prefs
 * @param {Object} action
 * @return {PreferencesState}
 */
function updatePrefs(prefs, action) {
  switch (action.name) {
    case 'set-incognito-availability':
      return /** @type {PreferencesState} */ (Object.assign({}, prefs, {
        incognitoAvailability: action.value,
      }));
    case 'set-can-edit':
      return /** @type {PreferencesState} */ (Object.assign({}, prefs, {
        canEdit: action.value,
      }));
    default:
      return prefs;
  }
}

/**
 * Root reducer for the Bookmarks page. This is called by the store in
 * response to an action, and the return value is used to update the UI.
 * @param {!BookmarksPageState} state
 * @param {Object} action
 * @return {!BookmarksPageState}
 */
export function reduceAction(state, action) {
  return {
    nodes: updateNodes(state.nodes, action),
    selectedFolder:
        updateSelectedFolder(state.selectedFolder, action, state.nodes),
    folderOpenState:
        updateFolderOpenState(state.folderOpenState, action, state.nodes),
    prefs: updatePrefs(state.prefs, action),
    search: updateSearch(state.search, action),
    selection: updateSelection(state.selection, action),
  };
}
