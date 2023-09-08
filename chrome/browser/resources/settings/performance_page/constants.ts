// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Must be kept in sync with the C++ enum of the same name (see
 * chrome/browser/preloading/preloading_prefs.h).
 */
export enum NetworkPredictionOptions {
  STANDARD = 0,
  WIFI_ONLY_DEPRECATED = 1,
  DISABLED = 2,
  EXTENDED = 3,
  DEFAULT = 1,
}
