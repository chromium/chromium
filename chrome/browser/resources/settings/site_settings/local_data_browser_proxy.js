// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the Cookies and Local Storage Data
 * section.
 */

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {CookieDetails} from './cookie_info.js';
// clang-format on

/**
 * @typedef {{
 *   localData: string,
 *   site: string,
 * }}
 */
export let LocalDataItem;

/**
 * Number of cookies attached to a given domain / eTLD+1.
 * @typedef {{
 *   etldPlus1: string,
 *   numCookies: number,
 * }}
 */
let EtldPlus1CookieNumber;

/** @interface */
export class LocalDataBrowserProxy {
  /**
   * @param {string} filter Search filter (use "" for none).
   * @return {!Promise<!Array<!LocalDataItem>>}
   */
  getDisplayList(filter) {}

  /**
   * Removes all local data (local storage, cookies, etc.).
   * Note: on-tree-item-removed will not be sent.
   * @return {!Promise} To signal completion.
   */
  removeAll() {}

  /**
   * Remove items that pass the current filter. Completion signaled by
   * on-tree-item-removed.
   */
  removeShownItems() {}

  /**
   * Remove data for a specific site. Completion signaled by
   * on-tree-item-removed.
   * @param {string} site Site to delete data for.
   */
  removeSite(site) {}

  /**
   * Gets the cookie details for a particular site.
   * @param {string} site The name of the site.
   * @return {!Promise<!Array<!CookieDetails>>}
   */
  getCookieDetails(site) {}

  /**
   * Gets the plural string for a given number of cookies.
   * @param {number} numCookies The number of cookies.
   * @return {!Promise<string>}
   */
  getNumCookiesString(numCookies) {}

  /**
   * Reloads all local data.
   * TODO(dschuyler): rename function to reload().
   * @return {!Promise} To signal completion.
   */
  reloadCookies() {}

  /**
   * Removes a given piece of site data.
   * @param {string} path The path to the item in the tree model.
   */
  removeItem(path) {}

  /**
   * Removes all SameSite=None cookies, as well as storage available in
   * third-party contexts.
   * Note: on-tree-item-removed will not be sent.
   * @return {!Promise} To signal completion.
   */
  removeAllThirdPartyCookies() {}
}

/**
 * @implements {LocalDataBrowserProxy}
 */
export class LocalDataBrowserProxyImpl {
  /** @override */
  getDisplayList(filter) {
    return sendWithPromise('localData.getDisplayList', filter);
  }

  /** @override */
  removeAll() {
    return sendWithPromise('localData.removeAll');
  }

  /** @override */
  removeShownItems() {
    chrome.send('localData.removeShownItems');
  }

  /** @override */
  removeSite(site) {
    chrome.send('localData.removeSite', [site]);
  }

  /** @override */
  getCookieDetails(site) {
    return sendWithPromise('localData.getCookieDetails', site);
  }

  /** @override */
  getNumCookiesString(numCookies) {
    return sendWithPromise('localData.getNumCookiesString', numCookies);
  }

  /** @override */
  reloadCookies() {
    return sendWithPromise('localData.reload');
  }

  /** @override */
  removeItem(path) {
    chrome.send('localData.removeItem', [path]);
  }

  /** @override */
  removeAllThirdPartyCookies() {
    return sendWithPromise('localData.removeThirdPartyCookies');
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
addSingletonGetter(LocalDataBrowserProxyImpl);
