// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module of functions which produce a new page state in response
 * to an action. Reducers (in the same sense as Array.prototype.reduce) must be
 * pure functions: they must not modify existing state objects, or make any API
 * calls.
 */

import {assert} from 'chrome://resources/js/assert.js';
import type {Action} from 'chrome://resources/js/store.js';

import type {ChangeFolderOpenAction, CreateBookmarkAction, EditBookmarkAction, FinishSearchAction, MoveBookmarkAction, RefreshNodesAction, RemoveBookmarkAction, ReorderChildrenAction, SelectFolderAction, SelectItemsAction, SetPrefAction, StartSearchAction, UpdateAnchorAction} from './actions.js';
import type {BookmarkNode, BookmarksPageState, FolderOpenState, NodeMap, PreferencesState, SearchState, SelectionState} from './types.js';
import {removeIdsFromMap, removeIdsFromObject, removeIdsFromSet} from './util.js';

function selectItems(
    selectionState: SelectionState, action: SelectItemsAction): SelectionState {
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

  return (Object.assign({}, selectionState, {
    items: newItems,
    anchor: action.anchor,
  }) as SelectionState);
}

function deselectAll(_selectionState: SelectionState): SelectionState {
  return {
    items: new Set(),
    anchor: null,
  };
}

function deselectItems(
    selectionState: SelectionState, deleted: Set<string>): SelectionState {
  return /** @type {SelectionState} */ (Object.assign({}, selectionState, {
    items: removeIdsFromSet(selectionState.items, deleted),
    anchor: !selectionState.anchor || deleted.has(selectionState.anchor) ?
        null :
        selectionState.anchor,
  }));
}

function updateAnchor(
    selectionState: SelectionState,
    action: UpdateAnchorAction): SelectionState {
  return (Object.assign({}, selectionState, {
    anchor: action.anchor,
  }) as SelectionState);
}

// Exported for tests.
export function updateSelection(
    selection: SelectionState, action: Action): SelectionState {
  switch (action.name) {
    case 'clear-search':
    case 'finish-search':
    case 'select-folder':
    case 'deselect-items':
      return deselectAll(selection);
    case 'select-items':
      return selectItems(selection, action as SelectItemsAction);
    case 'remove-bookmark':
      return deselectItems(
          selection, (action as RemoveBookmarkAction).descendants);
    case 'move-bookmark':
      // Deselect items when they are moved to another folder, since they will
      // no longer be visible on screen (for simplicity, ignores items visible
      // in search results).
      const moveAction = action as MoveBookmarkAction;
      if (moveAction.parentId !== moveAction.oldParentId &&
          selection.items.has(moveAction.id)) {
        return deselectItems(selection, new Set([moveAction.id]));
      }
      return selection;
    case 'update-anchor':
      return updateAnchor(selection, action as UpdateAnchorAction);
    default:
      return selection;
  }
}

function startSearch(
    search: SearchState, action: StartSearchAction): SearchState {
  return {
    term: action.term,
    inProgress: true,
    results: search.results,
  };
}

function finishSearch(
    search: SearchState, action: FinishSearchAction): SearchState {
  return /** @type {SearchState} */ (Object.assign({}, search, {
    inProgress: false,
    results: action.results,
  }));
}

function clearSearch(): SearchState {
  return {
    term: '',
    inProgress: false,
    results: null,
  };
}

function removeDeletedResults(
    search: SearchState, deletedIds: Set<string>): SearchState {
  if (!search.results) {
    return search;
  }

  const newResults: string[] = [];
  search.results.forEach(function(id) {
    if (!deletedIds.has(id)) {
      newResults.push(id);
    }
  });
  return (Object.assign({}, search, {
    results: newResults,
  }) as SearchState);
}

function updateSearch(search: SearchState, action: Action): SearchState {
  switch (action.name) {
    case 'start-search':
      return startSearch(search, action as StartSearchAction);
    case 'select-folder':
    case 'clear-search':
      return clearSearch();
    case 'finish-search':
      return finishSearch(search, action as FinishSearchAction);
    case 'remove-bookmark':
      return removeDeletedResults(
          search, (action as RemoveBookmarkAction).descendants);
    default:
      return search;
  }
}

function modifyNode(
    nodes: NodeMap, id: string,
    callback: (p1: BookmarkNode) => BookmarkNode): NodeMap {
  const nodeModification: NodeMap = {};
  nodeModification[id] = callback(nodes[id]!);
  return Object.assign({}, nodes, nodeModification);
}

function createBookmark(nodes: NodeMap, action: CreateBookmarkAction): NodeMap {
  const nodeModifications: NodeMap = {};
  nodeModifications[action.id] = action.node;

  const parentNode = nodes[action.parentId]!;
  const newChildren = parentNode.children!.slice();
  newChildren.splice(action.parentIndex, 0, action.id);
  nodeModifications[action.parentId] = Object.assign({}, parentNode, {
    children: newChildren,
  });

  return Object.assign({}, nodes, nodeModifications);
}

function editBookmark(nodes: NodeMap, action: EditBookmarkAction): NodeMap {
  // Do not allow folders to change URL (making them no longer folders).
  if (!nodes[action.id]!.url && action.changeInfo.url) {
    delete action.changeInfo.url;
  }

  return modifyNode(nodes, action.id, function(node) {
    return Object.assign({}, node, action.changeInfo);
  });
}

function moveBookmark(nodes: NodeMap, action: MoveBookmarkAction): NodeMap {
  const nodeModifications: NodeMap = {};
  const id = action.id;

  // Change node's parent.
  nodeModifications[id] =
      Object.assign({}, nodes[id], {parentId: action.parentId});

  // Remove from old parent.
  const oldParentId = action.oldParentId;
  const oldParentChildren = nodes[oldParentId]!.children!.slice();
  oldParentChildren.splice(action.oldIndex, 1);
  nodeModifications[oldParentId] =
      Object.assign({}, nodes[oldParentId], {children: oldParentChildren});

  // Add to new parent.
  const parentId = action.parentId;
  const parentChildren = oldParentId === parentId ?
      oldParentChildren :
      nodes[parentId]!.children!.slice();
  parentChildren.splice(action.index, 0, action.id);
  nodeModifications[parentId] =
      Object.assign({}, nodes[parentId], {children: parentChildren});

  return Object.assign({}, nodes, nodeModifications);
}

function removeBookmark(nodes: NodeMap, action: RemoveBookmarkAction): NodeMap {
  const newState = modifyNode(nodes, action.parentId, function(node) {
    const newChildren = node.children!.slice();
    newChildren.splice(action.index, 1);
    return /** @type {BookmarkNode} */ (
        Object.assign({}, node, {children: newChildren}));
  });

  return removeIdsFromObject(newState, action.descendants);
}

function reorderChildren(
    nodes: NodeMap, action: ReorderChildrenAction): NodeMap {
  return modifyNode(nodes, action.id, function(node) {
    return /** @type {BookmarkNode} */ (
        Object.assign({}, node, {children: action.children}));
  });
}

export function updateNodes(nodes: NodeMap, action: Action): NodeMap {
  switch (action.name) {
    case 'create-bookmark':
      return createBookmark(nodes, action as CreateBookmarkAction);
    case 'edit-bookmark':
      return editBookmark(nodes, action as EditBookmarkAction);
    case 'move-bookmark':
      return moveBookmark(nodes, action as MoveBookmarkAction);
    case 'remove-bookmark':
      return removeBookmark(nodes, action as RemoveBookmarkAction);
    case 'reorder-children':
      return reorderChildren(nodes, action as ReorderChildrenAction);
    case 'refresh-nodes':
      return (action as RefreshNodesAction).nodes;
    default:
      return nodes;
  }
}

function isAncestorOf(
    nodes: NodeMap, ancestorId: string, childId: string): boolean {
  let currentId: string|undefined = childId;
  // Work upwards through the tree from child.
  while (currentId) {
    if (currentId === ancestorId) {
      return true;
    }
    currentId = nodes[currentId!]!.parentId;
  }
  return false;
}

// Exported for tests.
export function updateSelectedFolder(
    selectedFolder: string, action: Action, nodes: NodeMap): string {
  switch (action.name) {
    case 'select-folder':
      return (action as SelectFolderAction).id;
    case 'change-folder-open':
      // When hiding the selected folder by closing its ancestor, select
      // that ancestor instead.
      const changeFolderAction = action as ChangeFolderOpenAction;
      if (!changeFolderAction.open && selectedFolder &&
          isAncestorOf(nodes, changeFolderAction.id, selectedFolder)) {
        return changeFolderAction.id;
      }
      return selectedFolder;
    case 'remove-bookmark':
      // When deleting the selected folder (or its ancestor), select the
      // parent of the deleted node.
      const id = (action as RemoveBookmarkAction).id;
      if (selectedFolder && isAncestorOf(nodes, id, selectedFolder)) {
        const parentId = nodes[id]!.parentId;
        assert(parentId);
        return parentId;
      }
      return selectedFolder;
    default:
      return selectedFolder;
  }
}

function openFolderAndAncestors(
    folderOpenState: FolderOpenState, id: string, nodes: NodeMap):
        FolderOpenState {
  const newFolderOpenState = (new Map(folderOpenState) as FolderOpenState);
  for (let currentId = id; currentId; currentId = nodes[currentId]!.parentId!) {
    newFolderOpenState.set(currentId, true);
  }

  return newFolderOpenState;
}

function changeFolderOpen(
    folderOpenState: FolderOpenState,
    action: ChangeFolderOpenAction): FolderOpenState {
  const newFolderOpenState = new Map(folderOpenState) as FolderOpenState;
  newFolderOpenState.set(action.id, action.open);

  return newFolderOpenState;
}

export function updateFolderOpenState(
    folderOpenState: FolderOpenState, action: Action,
    nodes: NodeMap): FolderOpenState {
  switch (action.name) {
    case 'change-folder-open':
      return changeFolderOpen(
          folderOpenState, action as ChangeFolderOpenAction);
    case 'select-folder':
      return openFolderAndAncestors(
          folderOpenState, nodes[(action as SelectFolderAction).id]!.parentId!,
          nodes);
    case 'move-bookmark':
      if (!nodes[(action as MoveBookmarkAction).id]!.children) {
        return folderOpenState;
      }
      return openFolderAndAncestors(
          folderOpenState, (action as MoveBookmarkAction).parentId, nodes);
    case 'remove-bookmark':
      return removeIdsFromMap(
          folderOpenState, (action as RemoveBookmarkAction).descendants);
    default:
      return folderOpenState;
  }
}

function updatePrefs(
    prefs: PreferencesState, action: Action): PreferencesState {
  const prefAction = action as SetPrefAction;
  switch (prefAction.name) {
    case 'set-incognito-availability':
      return /** @type {PreferencesState} */ (Object.assign({}, prefs, {
        incognitoAvailability: prefAction.value,
      }));
    case 'set-can-edit':
      return /** @type {PreferencesState} */ (Object.assign({}, prefs, {
        canEdit: prefAction.value,
      }));
    default:
      return prefs;
  }
}

export function reduceAction(
    state: BookmarksPageState, action: Action): BookmarksPageState {
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
