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
 *   colorDark: (!Array<number>|undefined),
 *   colorId: (number|undefined),
 *   colorLight: (!Array<number>|undefined),
 *   colorPicked: (!Array<number>|undefined),
 *   customBackgroundConfigured: boolean,
 *   customBackgroundDisabledByPolicy: boolean,
 *   iconBackgroundColor: !Array<number>,
 *   imageHorizontalAlignment: (string|undefined),
 *   imageTiling: (string|undefined),
 *   imageUrl: (string|undefined),
 *   imageVerticalAlignment: (string|undefined),
 *   isNtpBackgroundDark: boolean,
 *   logoColor: (!Array<number>|undefined),
 *   searchBox: (!{
 *     bg: !Array<number>,
 *     icon: !Array<number>,
 *     iconSelected: !Array<number>,
 *     placeholder: !Array<number>,
 *     resultsBg: !Array<number>,
 *     resultsBgHovered: !Array<number>,
 *     resultsBgSelected: !Array<number>,
 *     resultsDim: !Array<number>,
 *     resultsDimSelected: !Array<number>,
 *     resultsText: !Array<number>,
 *     resultsTextSelected: !Array<number>,
 *     resultsUrl: !Array<number>,
 *     resultsUrlSelected: !Array<number>,
 *     text: !Array<number>,
 *   }|undefined),
 *   textColorLightRgba: !Array<number>,
 *   textColorRgba: !Array<number>,
 *   themeId: (string|undefined),
 *   themeName: (string|undefined),
 *   useTitleContainer: boolean,
 *   useWhiteAddIcon: boolean,
 *   usingDefaultTheme: boolean,
 * }}
 */
let NtpTheme;

/** @type {?NtpTheme} */
window.chrome.embeddedSearch.newTabPage.ntpTheme;

/**
 * No params.
 */
window.chrome.embeddedSearch.newTabPage.useDefaultTheme;
