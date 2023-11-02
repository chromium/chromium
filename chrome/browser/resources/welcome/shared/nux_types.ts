// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface BookmarkListItem {
  id: number;
  name: string;
  icon: string;
  url: string;
}

export interface StepIndicatorModel {
  total: number;
  active: number;
}

/**
 * TODO(hcarmona): somehow reuse from
 * c/b/r/s/default_browser_page/default_browser_browser_proxy.js
 */
export interface DefaultBrowserInfo {
  canBeDefault: boolean;
  isDefault: boolean;
  isDisabledByPolicy: boolean;
  isUnknownError: boolean;
}
