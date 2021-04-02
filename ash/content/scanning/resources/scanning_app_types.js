// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for the state of `scanning-app`.
 * @enum {number}
 */
export const AppState = {
  GETTING_SCANNERS: 0,
  GOT_SCANNERS: 1,
  GETTING_CAPS: 2,
  READY: 3,
  SCANNING: 4,
  DONE: 5,
  CANCELING: 6,
  NO_SCANNERS: 7,
};

/**
 * Enum for the action taken after a completed scan. These values are persisted
 * to logs. Entries should not be renumbered and numeric values should never be
 * reused. These values must be kept in sync with the ScanCompleteAction enum in
 * /ash/content/scanning/scanning_uma.h.
 * @enum {number}
 */
export const ScanCompleteAction = {
  DONE_BUTTON_CLICKED: 0,
  FILES_APP_OPENED: 1,
  MEDIA_APP_OPENED: 2,
};

/**
 * @typedef {!Array<!ash.scanning.mojom.Scanner>}
 */
export let ScannerArr;
