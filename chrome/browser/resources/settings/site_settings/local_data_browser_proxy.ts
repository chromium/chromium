// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the Cookies and Local Storage Data
 * section.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {CookieDetails} from './cookie_info.js';
// clang-format on

export type LocalDataItem = {
  localData: string,
  site: string,
};

export interface LocalDataBrowserProxy {
  getDisplayList(filter: string): Promise<LocalDataItem[]>;

  /**
   * Removes all local data (local storage, cookies, etc.).
   * Note: on-tree-item-removed will not be sent.
   */
  removeAll(): Promise<void>;

  /**
   * Remove items that pass the current filter. Completion signaled by
   * on-tree-item-removed.
   */
  removeShownItems(): void;

  /**
   * Remove data for a specific site. Completion signaled by
   * on-tree-item-removed.
   */
  removeSite(site: string): void;

  /**
   * Gets the cookie details for a particular site.
   */
  getCookieDetails(site: string): Promise<CookieDetails[]>;

  /**
   * Gets the plural string for a given number of cookies.
   * @param numCookies The number of cookies.
   */
  getNumCookiesString(numCookies: number): Promise<string>;

  /**
   * Reloads all local data.
   * TODO(dschuyler): rename function to reload().
   */
  reloadCookies(): Promise<void>;

  /**
   * Removes a given piece of site data.
   * @param path The path to the item in the tree model.
   */
  removeItem(path: string): void;

  /**
   * Removes all SameSite=None cookies, as well as storage available in
   * third-party contexts.
   * Note: on-tree-item-removed will not be sent.
   */
  removeAllThirdPartyCookies(): Promise<void>;
}

export class LocalDataBrowserProxyImpl implements LocalDataBrowserProxy {
  getDisplayList(filter: string) {
    return sendWithPromise('localData.getDisplayList', filter);
  }

  removeAll() {
    return sendWithPromise('localData.removeAll');
  }

  removeShownItems() {
    chrome.send('localData.removeShownItems');
  }

  removeSite(site: string) {
    chrome.send('localData.removeSite', [site]);
  }

  getCookieDetails(site: string) {
    return sendWithPromise('localData.getCookieDetails', site);
  }

  getNumCookiesString(numCookies: number) {
    return sendWithPromise('localData.getNumCookiesString', numCookies);
  }

  reloadCookies() {
    return sendWithPromise('localData.reload');
  }

  removeItem(path: string) {
    chrome.send('localData.removeItem', [path]);
  }

  removeAllThirdPartyCookies() {
    return sendWithPromise('localData.removeThirdPartyCookies');
  }

  static getInstance(): LocalDataBrowserProxy {
    return instance || (instance = new LocalDataBrowserProxyImpl());
  }

  static setInstance(obj: LocalDataBrowserProxy) {
    instance = obj;
  }
}

let instance: LocalDataBrowserProxy|null = null;
