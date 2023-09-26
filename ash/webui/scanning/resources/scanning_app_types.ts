// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {ColorMode, FileType, MultiPageScanControllerRemote, PageSize, Scanner, ScannerCapabilities, SourceType} from './scanning.mojom-webui.js';

/**
 * Enum for the state of `scanning-app`.
 */
export enum AppState {
  GETTING_SCANNERS = 0,
  GOT_SCANNERS = 1,
  GETTING_CAPS = 2,
  SETTING_SAVED_SETTINGS = 3,
  READY = 4,
  SCANNING = 5,
  DONE = 6,
  CANCELING = 7,
  NO_SCANNERS = 8,
  MULTI_PAGE_NEXT_ACTION = 9,
  MULTI_PAGE_SCANNING = 10,
  MULTI_PAGE_CANCELING = 11,
}

/**
 * Enum for the action taken after a completed scan. These values are persisted
 * to logs. Entries should not be renumbered and numeric values should never be
 * reused. These values must be kept in sync with the ScanCompleteAction enum in
 * /ash/webui/scanning/scanning_uma.h.
 */
export enum ScanCompleteAction {
  DONE_BUTTON_CLICKED = 0,
  FILES_APP_OPENED = 1,
  MEDIA_APP_OPENED = 2,
}

/**
 * Maximum number of scanners allowed in saved scan settings.
 */
export const MAX_NUM_SAVED_SCANNERS = 20;

export interface ScannerCapabilitiesResponse {
  capabilities: ScannerCapabilities;
}

export interface ScannerInfo {
  token: UnguessableToken;
  displayName: string;
}

export interface ScannerSetting {
  name: string;
  lastScanDate: Date;
  sourceName: string;
  fileType: FileType;
  colorMode: ColorMode;
  pageSize: PageSize;
  resolutionDpi: number;
  multiPageScanChecked: boolean;
}

export interface ScanSettings {
  lastUsedScannerName: string;
  scanToPath: string;
  scanners: ScannerSetting[];
}

export interface StartMultiPageScanResponse {
  controller: MultiPageScanControllerRemote|null;
}

export interface ScanJobSettingsForMetrics {
  sourceType: SourceType;
  fileType: FileType;
  colorMode: ColorMode;
  pageSize: PageSize;
  resolution: number;
}

export interface ScannersReceivedResponse {
  scanners: Scanner[];
}

export interface SuccessResponse {
  success: boolean;
}
