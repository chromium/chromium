// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';


/**
 * The different pages that can be shown at a time.
 * Note: This must remain in sync with the page ids in manager.html!
 * @enum {string}
 */
export const Page = {
  LIST: 'items-list',
  DETAILS: 'details-view',
  ACTIVITY_LOG: 'activity-log',
  SHORTCUTS: 'keyboard-shortcuts',
  ERRORS: 'error-page',
};

/** @enum {string} */
export const Dialog = {
  OPTIONS: 'options',
};

/**
 * @typedef {{page: Page,
 *            extensionId: (string|undefined),
 *            subpage: (!Dialog|undefined)}}
 */
export let PageState;

/**
 * @param {!PageState} a
 * @param {!PageState} b
 * @return {boolean} Whether a and b are equal.
 */
function isPageStateEqual(a, b) {
  return a.page == b.page && a.subpage == b.subpage &&
      a.extensionId == b.extensionId;
}

/**
 * Regular expression that captures the leading slash, the content and the
 * trailing slash in three different groups.
 * @const {!RegExp}
 */
const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;

/**
 * A helper object to manage in-page navigations. Since the extensions page
 * needs to support different urls for different subpages (like the details
 * page), we use this object to manage the history and url conversions.
 */
export class NavigationHelper {
  constructor() {
    this.processRoute_();

    /** @private {number} */
    this.nextListenerId_ = 1;

    /** @private {!Map<number, function(!PageState)>} */
    this.listeners_ = new Map();

    /** @private {!PageState} */
    this.previousPage_;

    window.addEventListener('popstate', () => {
      this.notifyRouteChanged_(this.getCurrentPage());
    });
  }

  /** @private */
  get currentPath_() {
    return location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
  }

  /**
   * Going to /configureCommands and /shortcuts should land you on /shortcuts.
   * These are the only two supported routes, so all other cases will redirect
   * you to root path if not already on it.
   * @private
   */
  processRoute_() {
    if (this.currentPath_ == '/configureCommands' ||
        this.currentPath_ == '/shortcuts') {
      window.history.replaceState(
          undefined /* stateObject */, '', '/shortcuts');
    } else if (this.currentPath_ !== '/') {
      window.history.replaceState(undefined /* stateObject */, '', '/');
    }
  }

  /**
   * @return {!PageState} The page that should be displayed for the
   *     current URL.
   */
  getCurrentPage() {
    const search = new URLSearchParams(location.search);
    let id = search.get('id');
    if (id) {
      return {page: Page.DETAILS, extensionId: id};
    }
    id = search.get('activity');
    if (id) {
      return {page: Page.ACTIVITY_LOG, extensionId: id};
    }
    id = search.get('options');
    if (id) {
      return {page: Page.DETAILS, extensionId: id, subpage: Dialog.OPTIONS};
    }
    id = search.get('errors');
    if (id) {
      return {page: Page.ERRORS, extensionId: id};
    }

    if (this.currentPath_ == '/shortcuts') {
      return {page: Page.SHORTCUTS};
    }

    return {page: Page.LIST};
  }

  /**
   * Function to add subscribers.
   * @param {!function(!PageState)} listener
   * @return {number} A numerical ID to be used for removing the listener.
   */
  addListener(listener) {
    const nextListenerId = this.nextListenerId_++;
    this.listeners_.set(nextListenerId, listener);
    return nextListenerId;
  }

  /**
   * Remove a previously registered listener.
   * @param {number} id
   * @return {boolean} Whether a listener with the given ID was actually found
   *     and removed.
   */
  removeListener(id) {
    return this.listeners_.delete(id);
  }

  /**
   * Function to notify subscribers.
   * @private
   */
  notifyRouteChanged_(newPage) {
    this.listeners_.forEach((listener, id) => {
      listener(newPage);
    });
  }

  /**
   * @param {!PageState} newPage the page to navigate to.
   */
  navigateTo(newPage) {
    const currentPage = this.getCurrentPage();
    if (currentPage && isPageStateEqual(currentPage, newPage)) {
      return;
    }

    this.updateHistory(newPage, false /* replaceState */);
    this.notifyRouteChanged_(newPage);
  }

  /**
   * @param {!PageState} newPage the page to replace the current
   *     page with.
   */
  replaceWith(newPage) {
    this.updateHistory(newPage, true /* replaceState */);
    if (this.previousPage_ && isPageStateEqual(this.previousPage_, newPage)) {
      // Skip the duplicate history entry.
      history.back();
      return;
    }
    this.notifyRouteChanged_(newPage);
  }

  /**
   * Called when a page changes, and pushes state to history to reflect it.
   * @param {!PageState} entry
   * @param {boolean} replaceState
   */
  updateHistory(entry, replaceState) {
    let path;
    switch (entry.page) {
      case Page.LIST:
        path = '/';
        break;
      case Page.ACTIVITY_LOG:
        path = '/?activity=' + entry.extensionId;
        break;
      case Page.DETAILS:
        if (entry.subpage) {
          assert(entry.subpage == Dialog.OPTIONS);
          path = '/?options=' + entry.extensionId;
        } else {
          path = '/?id=' + entry.extensionId;
        }
        break;
      case Page.SHORTCUTS:
        path = '/shortcuts';
        break;
      case Page.ERRORS:
        path = '/?errors=' + entry.extensionId;
        break;
    }
    assert(path);
    const state = {url: path};
    const currentPage = this.getCurrentPage();
    const isDialogNavigation = currentPage.page == entry.page &&
        currentPage.extensionId == entry.extensionId;
    // Navigating to a dialog doesn't visually change pages; it just opens
    // a dialog. As such, we replace state rather than pushing a new state
    // on the stack so that hitting the back button doesn't just toggle the
    // dialog.
    if (replaceState || isDialogNavigation) {
      history.replaceState(state, '', path);
    } else {
      this.previousPage_ = currentPage;
      history.pushState(state, '', path);
    }
  }
}

export const navigation = new NavigationHelper();
