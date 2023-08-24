// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the Scanning App UI in ash/ to
 * provide access to the ScanningHandler which invokes functions that only exist
 * in chrome/.
 */

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

import {ScanCompleteAction} from './scanning_app_types.js';

/**
 * @typedef {{
 *   baseName: string,
 *   filePath: string,
 * }}
 */
export let SelectedPath;

/**
 * @typedef {{
 *   sourceType: ash.scanning.mojom.SourceType,
 *   fileType: ash.scanning.mojom.FileType,
 *   colorMode: ash.scanning.mojom.ColorMode,
 *   pageSize: ash.scanning.mojom.PageSize,
 *   resolution: number,
 * }}
 */
let ScanJobSettingsForMetrics;

/** @interface */
export class ScanningBrowserProxy {
  /** Initialize ScanningHandler. */
  initialize() {}

  /**
   * Requests the user to choose the directory to save scans.
   * @return {!Promise<!SelectedPath>}
   */
  requestScanToLocation() {}

  /**
   * Opens the Files app with the file |pathToFile| highlighted.
   * @param {string} pathToFile
   * @return {!Promise<boolean>} True if the file is found and Files app opens.
   */
  showFileInLocation(pathToFile) {}

  /**
   * Returns a localized, pluralized string for |name| based on |count|.
   * @param {string} name
   * @param {number} count
   * @return {!Promise<string>}
   */
  getPluralString(name, count) {}

  /**
   * Records the settings for a scan job.
   * @param {!ScanJobSettingsForMetrics} scanJobSettings
   */
  recordScanJobSettings(scanJobSettings) {}

  /**
   * Returns the MyFiles path for the current user.
   * @return {!Promise<string>}
   */
  getMyFilesPath() {}

  /**
   * Opens the Media app with the files specified in |filePaths|.
   * @param {!Array<string>} filePaths
   */
  openFilesInMediaApp(filePaths) {}

  /**
   * Records the action taken after a completed scan job.
   * @param {!ScanCompleteAction} action
   */
  recordScanCompleteAction(action) {}

  /**
   * Records the number of scan setting changes before a scan is initiated.
   * @param {number} numChanges
   */
  recordNumScanSettingChanges(numChanges) {}

  /**
   * Saves scan settings to the Prefs service.
   * @param {string} scanSettings
   */
  saveScanSettings(scanSettings) {}

  /**
   * Returns the saved scan settings from the Prefs service.
   * @return {!Promise<string>}
   */
  getScanSettings() {}

  /**
   * Validates that |filePath| exists on the local filesystem and returns its
   * display name. If |filePath| doesn't exist, return an empty SelectedPath.
   * @param {string} filePath
   * @return {!Promise<!SelectedPath>}
   */
  ensureValidFilePath(filePath) {}

  /**
   * Records the number of completed scans during a session of the Scan app
   * being open.
   * @param {number} numCompletedScans
   */
  recordNumCompletedScans(numCompletedScans) {}
}

/** @implements {ScanningBrowserProxy} */
export class ScanningBrowserProxyImpl {
  /** @override */
  initialize() {
    chrome.send('initialize');
  }

  /** @override */
  requestScanToLocation() {
    return sendWithPromise('requestScanToLocation');
  }

  /** @override */
  showFileInLocation(pathToFile) {
    return sendWithPromise('showFileInLocation', pathToFile);
  }

  /** @override */
  getPluralString(name, count) {
    return sendWithPromise('getPluralString', name, count);
  }

  /** @override */
  recordScanJobSettings(scanJobSettings) {
    chrome.send('recordScanJobSettings', [scanJobSettings]);
  }

  /** @override */
  getMyFilesPath() {
    return sendWithPromise('getMyFilesPath');
  }

  /** @override */
  openFilesInMediaApp(filePaths) {
    chrome.send('openFilesInMediaApp', [filePaths]);
  }

  /** @override */
  recordScanCompleteAction(action) {
    chrome.send('recordScanCompleteAction', [action]);
  }

  /** @override */
  recordNumScanSettingChanges(numChanges) {
    chrome.send('recordNumScanSettingChanges', [numChanges]);
  }

  /** @override */
  saveScanSettings(scanSettings) {
    chrome.send('saveScanSettings', [scanSettings]);
  }

  /** @override */
  getScanSettings() {
    return sendWithPromise('getScanSettings');
  }

  /** @override */
  ensureValidFilePath(filePath) {
    return sendWithPromise('ensureValidFilePath', filePath);
  }

  /** @override */
  recordNumCompletedScans(numCompletedScans) {
    chrome.send('recordNumCompletedScans', [numCompletedScans]);
  }

  /** @return {!ScanningBrowserProxy} */
  static getInstance() {
    return instance || (instance = new ScanningBrowserProxyImpl());
  }

  /** @param {!ScanningBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?ScanningBrowserProxy} */
let instance = null;
