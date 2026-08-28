// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener, removeWebUiListener} from 'chrome://resources/js/cr.js';
import type {Action} from 'chrome://resources/js/store.js';

import {createBookmark, editBookmark, moveBookmark, removeBookmark, reorderChildren, setCanEditBookmarks, setIncognitoAvailability} from './actions.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {IncognitoAvailability} from './constants.js';
import {Debouncer} from './debouncer.js';
import {Store} from './store.js';
import type {BookmarkNode} from './types.js';

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
  if (!debouncer) {
    highlightUpdatedItemsImpl();
    return;
  }
  debouncer.promise.then(highlightUpdatedItemsImpl);
}

function dispatch(action: Action) {
  Store.getInstance().dispatch(action);
}

function onBookmarkChanged(id: string, node: BookmarkNode) {
  dispatch(editBookmark(id, {title: node.title, url: node.url}));
}

function onBookmarkCreated(
    parentId: string, index: number, node: BookmarkNode) {
  batchUIUpdates();
  if (trackUpdates) {
    updatedItems.push(node.id);
  }
  dispatch(createBookmark(parentId, index, node));
}

function onBookmarkRemoved(id: string, parentId: string, index: number) {
  batchUIUpdates();
  const nodes = Store.getInstance().data.nodes;
  dispatch(removeBookmark(id, parentId, index, nodes));
}

function onBookmarkMoved(
    id: string, oldParentId: string, oldIndex: number, newParentId: string,
    newIndex: number) {
  batchUIUpdates();
  if (trackUpdates) {
    updatedItems.push(id);
  }
  dispatch(moveBookmark(id, newParentId, newIndex, oldParentId, oldIndex));
}

function onChildrenReordered(id: string, reorderInfo: {childIds: string[]}) {
  dispatch(reorderChildren(id, reorderInfo.childIds));
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
  const apiProxy = BookmarksApiProxyImpl.getInstance();
  apiProxy.onChanged.addListener(onBookmarkChanged);
  apiProxy.onChildrenReordered.addListener(onChildrenReordered);
  apiProxy.onCreated.addListener(onBookmarkCreated);
  apiProxy.onMoved.addListener(onBookmarkMoved);
  apiProxy.onRemoved.addListener(onBookmarkRemoved);

  const browserProxy = BrowserProxyImpl.getInstance();
  browserProxy.getIncognitoAvailability().then(onIncognitoAvailabilityChanged);
  incognitoAvailabilityListener = addWebUiListener(
      'incognito-availability-changed', onIncognitoAvailabilityChanged);

  browserProxy.getCanEditBookmarks().then(onCanEditBookmarksChanged);
  canEditBookmarksListener =
      addWebUiListener('can-edit-bookmarks-changed', onCanEditBookmarksChanged);
}

export function destroy() {
  const apiProxy = BookmarksApiProxyImpl.getInstance();
  apiProxy.onChanged.removeListener(onBookmarkChanged);
  apiProxy.onChildrenReordered.removeListener(onChildrenReordered);
  apiProxy.onCreated.removeListener(onBookmarkCreated);
  apiProxy.onMoved.removeListener(onBookmarkMoved);
  apiProxy.onRemoved.removeListener(onBookmarkRemoved);
  if (incognitoAvailabilityListener) {
    removeWebUiListener(incognitoAvailabilityListener);
  }
  if (canEditBookmarksListener) {
    removeWebUiListener(canEditBookmarksListener);
  }
}

export function setDebouncerForTesting() {
  debouncer = new Debouncer(() => {});
}
