// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener, removeWebUiListener} from 'chrome://resources/js/cr.js';
import type {Action} from 'chrome://resources/js/store.js';

import {createBookmark, editBookmark, moveBookmark, refreshNodes, removeBookmark, reorderChildren, setCanEditBookmarks, setIncognitoAvailability} from './actions.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {IncognitoAvailability} from './constants.js';
import {Debouncer} from './debouncer.js';
import {Store} from './store.js';
import {normalizeNodes} from './util.js';

/**
 * @fileoverview Listener functions which translate events from the
 * chrome.bookmarks API into actions to modify the local page state.
 */

let trackUpdates: boolean = false;
let updatedItems: string[] = [];

let debouncer: Debouncer|null = null;

/**
 * Batches UI updates so that no changes will be made to UI until the next
 * task after the last call to this method. This is useful for listeners which
 * can be called in a tight loop by UI actions.
 */
function batchUIUpdates() {
  if (debouncer === null) {
    debouncer = new Debouncer(() => Store.getInstance().endBatchUpdate());
  }

  if (debouncer.done()) {
    Store.getInstance().beginBatchUpdate();
    debouncer.reset();
  }

  debouncer.restartTimeout();
}

/**
 * Tracks any items that are created or moved.
 */
export function trackUpdatedItems() {
  trackUpdates = true;
}

function highlightUpdatedItemsImpl() {
  if (!trackUpdates) {
    return;
  }

  document.dispatchEvent(new CustomEvent('highlight-items', {
    detail: updatedItems,
  }));
  updatedItems = [];
  trackUpdates = false;
}

/**
 * Highlights any items that have been updated since |trackUpdatedItems| was
 * called. Should be called after a user action causes new items to appear in
 * the main list.
 */
export function highlightUpdatedItems() {
  // Ensure that the items are highlighted after the current batch update (if
  // there is one) is completed.
  assert(debouncer);
  debouncer.promise.then(highlightUpdatedItemsImpl);
}

function dispatch(action: Action) {
  Store.getInstance().dispatch(action);
}

function onBookmarkChanged(
    id: string, changeInfo: chrome.bookmarks.ChangeInfo) {
  dispatch(editBookmark(id, changeInfo));
}

function onBookmarkCreated(
    id: string, treeNode: chrome.bookmarks.BookmarkTreeNode) {
  batchUIUpdates();
  if (trackUpdates) {
    updatedItems.push(id);
  }
  dispatch(createBookmark(id, treeNode));
}

function onBookmarkRemoved(
    id: string, removeInfo: chrome.bookmarks.RemoveInfo) {
  batchUIUpdates();
  const nodes = Store.getInstance().data.nodes;
  dispatch(removeBookmark(id, removeInfo.parentId, removeInfo.index, nodes));
}

function onBookmarkMoved(id: string, moveInfo: chrome.bookmarks.MoveInfo) {
  batchUIUpdates();
  if (trackUpdates) {
    updatedItems.push(id);
  }
  dispatch(moveBookmark(
      id, moveInfo.parentId, moveInfo.index, moveInfo.oldParentId,
      moveInfo.oldIndex));
}

function onChildrenReordered(
    id: string, reorderInfo: chrome.bookmarks.ReorderInfo) {
  dispatch(reorderChildren(id, reorderInfo.childIds));
}

/**
 * Pauses the Created handler during an import. The imported nodes will all be
 * loaded at once when the import is finished.
 */
function onImportBegan() {
  chrome.bookmarks.onCreated.removeListener(onBookmarkCreated);
  document.dispatchEvent(new CustomEvent('import-began'));
}

function onImportEnded() {
  chrome.bookmarks.getTree().then((results) => {
    dispatch(refreshNodes(normalizeNodes(results[0]!)));
  });
  chrome.bookmarks.onCreated.addListener(onBookmarkCreated);
  document.dispatchEvent(new CustomEvent('import-ended'));
}

function onIncognitoAvailabilityChanged(availability: IncognitoAvailability) {
  dispatch(setIncognitoAvailability(availability));
}

function onCanEditBookmarksChanged(canEdit: boolean) {
  dispatch(setCanEditBookmarks(canEdit));
}

let incognitoAvailabilityListener: {eventName: string, uid: number}|null = null;

let canEditBookmarksListener: {eventName: string, uid: number}|null = null;

export function init() {
  chrome.bookmarks.onChanged.addListener(onBookmarkChanged);
  chrome.bookmarks.onChildrenReordered.addListener(onChildrenReordered);
  chrome.bookmarks.onCreated.addListener(onBookmarkCreated);
  chrome.bookmarks.onMoved.addListener(onBookmarkMoved);
  chrome.bookmarks.onRemoved.addListener(onBookmarkRemoved);
  chrome.bookmarks.onImportBegan.addListener(onImportBegan);
  chrome.bookmarks.onImportEnded.addListener(onImportEnded);

  const browserProxy = BrowserProxyImpl.getInstance();
  browserProxy.getIncognitoAvailability().then(onIncognitoAvailabilityChanged);
  incognitoAvailabilityListener = addWebUiListener(
      'incognito-availability-changed', onIncognitoAvailabilityChanged);

  browserProxy.getCanEditBookmarks().then(onCanEditBookmarksChanged);
  canEditBookmarksListener =
      addWebUiListener('can-edit-bookmarks-changed', onCanEditBookmarksChanged);
}

export function destroy() {
  chrome.bookmarks.onChanged.removeListener(onBookmarkChanged);
  chrome.bookmarks.onChildrenReordered.removeListener(onChildrenReordered);
  chrome.bookmarks.onCreated.removeListener(onBookmarkCreated);
  chrome.bookmarks.onMoved.removeListener(onBookmarkMoved);
  chrome.bookmarks.onRemoved.removeListener(onBookmarkRemoved);
  chrome.bookmarks.onImportBegan.removeListener(onImportBegan);
  chrome.bookmarks.onImportEnded.removeListener(onImportEnded);
  if (incognitoAvailabilityListener) {
    removeWebUiListener(/** @type {{eventName: string, uid: number}} */ (
        incognitoAvailabilityListener));
  }
  if (canEditBookmarksListener) {
    removeWebUiListener(/** @type {{eventName: string, uid: number}} */ (
        canEditBookmarksListener));
  }
}

export function setDebouncerForTesting() {
  debouncer = new Debouncer(() => {});
}
