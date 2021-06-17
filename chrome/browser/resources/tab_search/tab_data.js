// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Token} from 'chrome://resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import {RecentlyClosedTab, Tab, TabGroup} from './tab_search.mojom-webui.js';

/** @enum {number} */
export const TabItemType = {
  OPEN: 1,
  RECENTLY_CLOSED: 2,
};

/**
 * TabData contains tabSearch.mojom.Tab and data derived from it.
 * It makes tabSearch.mojom.Tab immutable and works well for closure compiler
 * type checking.
 */
export class TabData {
  constructor() {
    /** @type {!Tab|!RecentlyClosedTab} */
    this.tab;

    /** @type {string} */
    this.hostname;

    /** @type {!Array<!{start: number, length: number}>|undefined} */
    this.titleHighlightRanges;

    /** @type {!Array<!{start: number, length: number}>|undefined} */
    this.hostnameHighlightRanges;

    /** @type {boolean} */
    this.inActiveWindow;

    /** @type {!TabItemType} */
    this.type;

    /** @type {string} */
    this.a11yTypeText;

    /** @type {?TabGroup} */
    this.tabGroup;
  }
}

/**
 * Converts a token to a string by combining the high and low values as strings
 * with a hashtag as the separator.
 * @param {!Token} token
 * @return {string}
 */
export function tokenToString(token) {
  return `${token.high.toString()}#${token.low.toString()}`;
}

/**
 * @param {!TabData} tabData
 * @return {string}
 */
export function ariaLabel(tabData) {
  const groupTitleOrEmpty = tabData.tabGroup ? tabData.tabGroup.title : '';
  return `${tabData.tab.title} ${groupTitleOrEmpty} ${tabData.hostname} ${
      tabData.tab.lastActiveElapsedText} ${tabData.a11yTypeText}`;
}
