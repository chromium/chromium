// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Clear browsing data" dialog
 * to interact with the browser.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * An InstalledApp represents a domain with data that the user might want
 * to protect from being deleted.
 */
export interface InstalledApp {
  registerableDomain: string;
  reasonBitfield: number;
  exampleOrigin: string;
  isChecked: boolean;
  storageSize: number;
  hasNotifications: boolean;
  appName: string;
}

/**
 * ClearBrowsingDataResult contains any possible follow-up notices that should
 * be shown to the user.
 */
export interface ClearBrowsingDataResult {
  showHistoryNotice: boolean;
  showPasswordsNotice: boolean;
}

/**
 * UpdateSyncStateEvent contains relevant information for a summary of a user's
 * updated Sync State.
 */
export interface UpdateSyncStateEvent {
  signedIn: boolean;
  syncConsented: boolean;
  syncingHistory: boolean;
  shouldShowCookieException: boolean;
  isNonGoogleDse: boolean;
  nonGoogleSearchHistoryString: string;
}

export interface ClearBrowsingDataBrowserProxy {
  /**
   * @return A promise resolved when data clearing has completed. The boolean
   *     indicates whether an additional dialog should be shown, informing the
   *     user about other forms of browsing history.
   */
  clearBrowsingData(
      dataTypes: string[], timePeriod: number,
      installedApps: InstalledApp[]): Promise<ClearBrowsingDataResult>;

  /**
   * @return A promise resolved after fetching all installed apps. The array
   *     will contain a list of origins for which there are installed apps.
   */
  getInstalledApps(timePeriod: number): Promise<InstalledApp[]>;

  /**
   * Kick off counter updates and return initial state.
   * @return Signal when the setup is complete.
   */
  initialize(): Promise<void>;

  /**
   * @return A promise with the current sync state.
   */
  getSyncState(): Promise<UpdateSyncStateEvent>;
}

export class ClearBrowsingDataBrowserProxyImpl implements
    ClearBrowsingDataBrowserProxy {
  clearBrowsingData(
      dataTypes: string[], timePeriod: number, installedApps: InstalledApp[]) {
    return sendWithPromise(
        'clearBrowsingData', dataTypes, timePeriod, installedApps);
  }

  getInstalledApps(timePeriod: number) {
    return sendWithPromise('getInstalledApps', timePeriod);
  }

  initialize() {
    return sendWithPromise('initializeClearBrowsingData');
  }

  getSyncState() {
    return sendWithPromise('getSyncState');
  }

  static getInstance(): ClearBrowsingDataBrowserProxy {
    return instance || (instance = new ClearBrowsingDataBrowserProxyImpl());
  }

  static setInstance(obj: ClearBrowsingDataBrowserProxy) {
    instance = obj;
  }
}

let instance: ClearBrowsingDataBrowserProxy|null = null;
