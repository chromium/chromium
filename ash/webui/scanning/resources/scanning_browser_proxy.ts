// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the Scanning App UI in ash/ to
 * provide access to the ScanningHandler which invokes functions that only exist
 * in chrome/.
 */

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

import {ScanCompleteAction, ScanJobSettingsForMetrics} from './scanning_app_types.js';

export interface SelectedPath {
  baseName: string;
  filePath: string;
}

export interface ScanningBrowserProxy {
  initialize(): void;

  /**
   * Requests the user to choose the directory to save scans.
   */
  requestScanToLocation(): Promise<SelectedPath>;

  /**
   * Opens the Files app with the file |pathToFile| highlighted. Returns true
   * if the file is found and Files app opens.
   */
  showFileInLocation(pathToFile: string): Promise<boolean>;

  /**
   * Returns a localized, pluralized string for |name| based on |count|.
   */
  getPluralString(name: string, count: number): Promise<string>;

  /**
   * Records the settings for a scan job.
   */
  recordScanJobSettings(scanJobSettings: ScanJobSettingsForMetrics): void;

  /**
   * Returns the MyFiles path for the current user.
   */
  getMyFilesPath(): Promise<string>;

  /**
   * Opens the Media app with the files specified in |filePaths|.
   */
  openFilesInMediaApp(filePaths: string[]): void;

  /**
   * Records the action taken after a completed scan job.
   */
  recordScanCompleteAction(action: ScanCompleteAction): void;

  /**
   * Records the number of scan setting changes before a scan is initiated.
   */
  recordNumScanSettingChanges(numChanges: number): void;

  /**
   * Saves scan settings to the Prefs service.
   */
  saveScanSettings(scanSettings: string): void;

  /**
   * Returns the saved scan settings from the Prefs service.
   */
  getScanSettings(): Promise<string>;

  /**
   * Validates that |filePath| exists on the local filesystem and returns its
   * display name. If |filePath| doesn't exist, return an empty SelectedPath.
   */
  ensureValidFilePath(filePath: string): Promise<SelectedPath>;

  /**
   * Records the number of completed scans during a session of the Scan app
   * being open.
   */
  recordNumCompletedScans(numCompletedScans: number): void;
}

export class ScanningBrowserProxyImpl implements ScanningBrowserProxy {
  initialize(): void {
    chrome.send('initialize');
  }

  requestScanToLocation(): Promise<SelectedPath> {
    return sendWithPromise('requestScanToLocation');
  }

  showFileInLocation(pathToFile: string): Promise<boolean> {
    return sendWithPromise('showFileInLocation', pathToFile);
  }

  getPluralString(name: string, count: number): Promise<string> {
    return sendWithPromise('getPluralString', name, count);
  }

  recordScanJobSettings(scanJobSettings: ScanJobSettingsForMetrics): void {
    chrome.send('recordScanJobSettings', [scanJobSettings]);
  }

  getMyFilesPath(): Promise<string> {
    return sendWithPromise('getMyFilesPath');
  }

  openFilesInMediaApp(filePaths: string[]): void {
    chrome.send('openFilesInMediaApp', [filePaths]);
  }

  recordScanCompleteAction(action: ScanCompleteAction): void {
    chrome.send('recordScanCompleteAction', [action]);
  }

  recordNumScanSettingChanges(numChanges: number): void {
    chrome.send('recordNumScanSettingChanges', [numChanges]);
  }

  saveScanSettings(scanSettings: string): void {
    chrome.send('saveScanSettings', [scanSettings]);
  }

  getScanSettings(): Promise<string> {
    return sendWithPromise('getScanSettings');
  }

  ensureValidFilePath(filePath: string): Promise<SelectedPath> {
    return sendWithPromise('ensureValidFilePath', filePath);
  }

  recordNumCompletedScans(numCompletedScans: number): void {
    chrome.send('recordNumCompletedScans', [numCompletedScans]);
  }

  static getInstance(): ScanningBrowserProxy {
    return instance || (instance = new ScanningBrowserProxyImpl());
  }

  static setInstance(obj: ScanningBrowserProxy): void {
    instance = obj;
  }
}

let instance: ScanningBrowserProxy|null = null;
