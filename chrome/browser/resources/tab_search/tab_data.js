// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Tab} from './tab_search.mojom-webui.js';

/**
 * TabData contains tabSearch.mojom.Tab and data derived from it.
 * It makes tabSearch.mojom.Tab immutable and works well for closure compiler
 * type checking.
 */
export class TabData {
  constructor() {
    /** @type {!Tab} */
    this.tab;

    /** @type {string} */
    this.hostname;

    /** @type {!Array<!{start: number, length: number}>|undefined} */
    this.titleHighlightRanges;

    /** @type {!Array<!{start: number, length: number}>|undefined} */
    this.hostnameHighlightRanges;

    /** @type {boolean} */
    this.inActiveWindow;
  }
}
