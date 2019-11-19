// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   id: number,
 *   name: string,
 *   icon: string,
 *   url: string,
 * }}
 */
export let BookmarkListItem;

/**
 * @typedef {{
 *   total: number,
 *   active: number,
 * }}
 */
export let stepIndicatorModel;

/**
 * TODO(hcarmona): somehow reuse from
 * c/b/r/s/default_browser_page/default_browser_browser_proxy.js
 * @typedef {{
 *   canBeDefault: boolean,
 *   isDefault: boolean,
 *   isDisabledByPolicy: boolean,
 *   isUnknownError: boolean,
 * }};
 */
export let DefaultBrowserInfo;
