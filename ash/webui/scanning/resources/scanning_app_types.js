// Copyright 2020 The Chromium Authors
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
  SETTING_SAVED_SETTINGS: 3,
  READY: 4,
  SCANNING: 5,
  DONE: 6,
  CANCELING: 7,
  NO_SCANNERS: 8,
  MULTI_PAGE_NEXT_ACTION: 9,
  MULTI_PAGE_SCANNING: 10,
  MULTI_PAGE_CANCELING: 11,
};

/**
 * Enum for the action taken after a completed scan. These values are persisted
 * to logs. Entries should not be renumbered and numeric values should never be
 * reused. These values must be kept in sync with the ScanCompleteAction enum in
 * /ash/webui/scanning/scanning_uma.h.
 * @enum {number}
 */
export const ScanCompleteAction = {
  DONE_BUTTON_CLICKED: 0,
  FILES_APP_OPENED: 1,
  MEDIA_APP_OPENED: 2,
};

/**
 * Maximum number of scanners allowed in saved scan settings.
 * @const {number}
 */
export const MAX_NUM_SAVED_SCANNERS = 20;

/**
 * @typedef {!Array<!ash.scanning.mojom.Scanner>}
 */
export let ScannerArr;

/**
 * @typedef {{capabilities: !ash.scanning.mojom.ScannerCapabilities}}
 */
export let ScannerCapabilitiesResponse;

/**
 * @typedef {{
 *   token: !mojoBase.mojom.UnguessableToken,
 *   displayName: string,
 * }}
 */
export let ScannerInfo;

/**
 * @typedef {{
 *   name: string,
 *   lastScanDate: !Date,
 *   sourceName: string,
 *   fileType: ash.scanning.mojom.FileType,
 *   colorMode: ash.scanning.mojom.ColorMode,
 *   pageSize: ash.scanning.mojom.PageSize,
 *   resolutionDpi: number,
 *   multiPageScanChecked: boolean,
 * }}
 */
export let ScannerSetting;

/**
 * @typedef {{
 *   lastUsedScannerName: string,
 *   scanToPath: string,
 *   scanners: !Array<ScannerSetting>,
 * }}
 */
export let ScanSettings;

/**
 * @typedef {{controller:
                     ?ash.scanning.mojom.MultiPageScanControllerRemote}}
 */
export let StartMultiPageScanResponse;

/**
 * @typedef {!ash.common.mojom.ForceHiddenElementsVisibleObserverInterface}
 */
export let ForceHiddenElementsVisibleObserverInterface;
