/**
 * @license
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @fileoverview Externs for objects sent from C++ to
 * chrome-search://most-visited/title.html.
 * @externs
 */

/**************************** Embedded Search API ****************************/

/**
 * Embedded Search API methods defined in
 * chrome/renderer/searchbox/searchbox_extension.cc:
 *  NewTabPageBindings::GetObjectTemplateBuilder()
 */

window.chrome;
window.chrome.embeddedSearch;
window.chrome.embeddedSearch.newTabPage;

/**
 * @param {number} rid
 */
window.chrome.embeddedSearch.newTabPage.getMostVisitedItemData;

/**
 * @typedef {{
 *   alternateLogo: boolean,
 *   attribution1: (string|undefined),
 *   attribution2: (string|undefined),
 *   attributionActionUrl: (string|undefined),
 *   attributionUrl: (string|undefined),
 *   backgroundColorRgba: !Array<number>,
 *   collectionId: (string|undefined),
 *   customBackgroundConfigured: boolean,
 *   imageHorizontalAlignment: (string|undefined),
 *   imageTiling: (string|undefined),
 *   imageUrl: (string|undefined),
 *   imageVerticalAlignment: (string|undefined),
 *   textColorLightRgba: !Array<number>,
 *   textColorRgba: !Array<number>,
 *   usingDefaultTheme: boolean,
 * }}
 */
let NtpTheme;

/** @type {?NtpTheme} */
window.chrome.embeddedSearch.newTabPage.ntpTheme;
