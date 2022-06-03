// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type BookmarkListItem = {
  id: number,
  name: string,
  icon: string,
  url: string,
};

export type stepIndicatorModel = {
  total: number,
  active: number,
};

/**
 * TODO(hcarmona): somehow reuse from
 * c/b/r/s/default_browser_page/default_browser_browser_proxy.js
 */
export type DefaultBrowserInfo = {
  canBeDefault: boolean,
  isDefault: boolean,
  isDisabledByPolicy: boolean,
  isUnknownError: boolean,
};
