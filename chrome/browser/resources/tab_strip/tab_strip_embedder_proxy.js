// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';

export class TabStripEmbedderProxy {
  /** @return {boolean} */
  isVisible() {
    return document.visibilityState === 'visible';
  }

  /**
   * @return {!Promise<!Object<string, string>>} Object with CSS variables
   *     as keys and rgba strings as values
   */
  getColors() {
    return sendWithPromise('getThemeColors');
  }

  /**
   * @return {!Promise<!Object<string, string>>} Object with CSS variables
   *     as keys and pixel lengths as values
   */
  getLayout() {
    return sendWithPromise('getLayout');
  }

  observeThemeChanges() {
    chrome.send('observeThemeChanges');
  }

  /**
   * @param {number} tabId
   * @param {number} locationX
   * @param {number} locationY
   */
  showTabContextMenu(tabId, locationX, locationY) {
    chrome.send('showTabContextMenu', [tabId, locationX, locationY]);
  }

  closeContainer() {
    chrome.send('closeContainer');
  }
}

addSingletonGetter(TabStripEmbedderProxy);
