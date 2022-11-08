// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface StartupPageInfo {
  modelIndex: number;
  title: string;
  tooltip: string;
  url: string;
}

export interface StartupUrlsPageBrowserProxy {
  loadStartupPages(): void;
  useCurrentPages(): void;

  /** @return Whether the URL is valid. */
  validateStartupPage(url: string): Promise<boolean>;

  /**
   * @return Whether the URL was actually added, or ignored because it was
   *     invalid.
   */
  addStartupPage(url: string): Promise<boolean>;

  /**
   * @return Whether the URL was actually edited, or ignored because it was
   *     invalid.
   */
  editStartupPage(modelIndex: number, url: string): Promise<boolean>;

  removeStartupPage(index: number): void;
}

export class StartupUrlsPageBrowserProxyImpl implements
    StartupUrlsPageBrowserProxy {
  loadStartupPages() {
    chrome.send('onStartupPrefsPageLoad');
  }

  useCurrentPages() {
    chrome.send('setStartupPagesToCurrentPages');
  }

  validateStartupPage(url: string) {
    return sendWithPromise('validateStartupPage', url);
  }

  addStartupPage(url: string) {
    return sendWithPromise('addStartupPage', url);
  }

  editStartupPage(modelIndex: number, url: string) {
    return sendWithPromise('editStartupPage', modelIndex, url);
  }

  removeStartupPage(index: number) {
    chrome.send('removeStartupPage', [index]);
  }

  static getInstance(): StartupUrlsPageBrowserProxy {
    return instance || (instance = new StartupUrlsPageBrowserProxyImpl());
  }

  static setInstance(obj: StartupUrlsPageBrowserProxy) {
    instance = obj;
  }
}

let instance: StartupUrlsPageBrowserProxy|null = null;
