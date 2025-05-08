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

// Keep in sync with the respective enum in
// components/browsing_data/core/browsing_data_utils.h, and leave out values
// that are not available on Desktop.
// LINT.IfChange(TimePeriod)
export enum TimePeriod {
  LAST_HOUR = 0,
  LAST_DAY = 1,
  LAST_WEEK = 2,
  FOUR_WEEKS = 3,
  ALL_TIME = 4,
  // OLDER_THAN_30_DAYS = 5 is not used on Desktop.
  LAST_15_MINUTES = 6,
  TIME_PERIOD_LAST = LAST_15_MINUTES
}
// LINT.ThenChange(/components/browsing_data/core/browsing_data_utils.h:TimePeriod)

// Keep in sync with the respective enum in
// components/browsing_data/core/browsing_data_utils.h, and leave out values
// that are not available on Desktop.
// This enum represents ClearBrowsingDataDialogV2 and does not match the
// datatypes in the old dialog.
// LINT.IfChange(BrowsingDataType)
export enum BrowsingDataType {
  HISTORY = 0,
  CACHE = 1,
  SITE_DATA = 2,
  // PASSWORDS = 3, Not used on Desktop.
  FORM_DATA = 4,
  SITE_SETTINGS = 5,
  DOWNLOADS = 6,
  HOSTED_APPS_DATA = 7,
  // TABS = 8, Not used on Desktop.
}
// LINT.ThenChange(/components/browsing_data/core/browsing_data_utils.h:BrowsingDataType)

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
  clearBrowsingData(dataTypes: string[], timePeriod: number):
      Promise<ClearBrowsingDataResult>;

  /**
   * Kick off counter updates and return initial state.
   * @return Signal when the setup is complete.
   */
  initialize(): Promise<void>;

  /**
   * @return A promise with the current sync state.
   */
  getSyncState(): Promise<UpdateSyncStateEvent>;

  /**
   * Requests the backend to restart the browsing data counters of the basic or
   * advanced tab (determined by |isBasic|), instructing them to calculate the
   * data volume for the |timePeriod|. No return value, as the frontend needn't
   * wait for the counting to be completed.
   */
  restartCounters(isBasic: boolean, timePeriod: number): void;

  recordSettingsClearBrowsingDataBasicTimePeriodHistogram(bucket: TimePeriod):
      void;

  recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram(
      bucket: TimePeriod): void;
}

export class ClearBrowsingDataBrowserProxyImpl implements
    ClearBrowsingDataBrowserProxy {
  clearBrowsingData(dataTypes: string[], timePeriod: number) {
    return sendWithPromise('clearBrowsingData', dataTypes, timePeriod);
  }

  initialize() {
    return sendWithPromise('initializeClearBrowsingData');
  }

  getSyncState() {
    return sendWithPromise('getSyncState');
  }

  restartCounters(isBasic: boolean, timePeriod: number) {
    chrome.send('restartClearBrowsingDataCounters', [isBasic, timePeriod]);
  }

  recordSettingsClearBrowsingDataBasicTimePeriodHistogram(bucket: TimePeriod) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.ClearBrowsingData.Basic.TimePeriod',
      bucket,
      TimePeriod.TIME_PERIOD_LAST,
    ]);
  }

  recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram(bucket:
                                                                 TimePeriod) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.ClearBrowsingData.Advanced.TimePeriod',
      bucket,
      TimePeriod.TIME_PERIOD_LAST,
    ]);
  }

  static getInstance(): ClearBrowsingDataBrowserProxy {
    return instance || (instance = new ClearBrowsingDataBrowserProxyImpl());
  }

  static setInstance(obj: ClearBrowsingDataBrowserProxy) {
    instance = obj;
  }
}

let instance: ClearBrowsingDataBrowserProxy|null = null;
