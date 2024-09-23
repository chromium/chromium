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
export enum TimePeriod {
  LAST_HOUR = 0,
  LAST_DAY = 1,
  LAST_WEEK = 2,
  FOUR_WEEKS = 3,
  ALL_TIME = 4,
  // OLDER_THAN_30_DAYS = 5 is not used on Desktop.
  // LAST_15_MINUTES = 6 is not used on Desktop.
  TIME_PERIOD_LAST = ALL_TIME
}

// TODO(crbug.com/40283307): Remove this after CbdTimeframeRequired finishes.
// Keep in sync with the respective enum in
// components/browsing_data/core/browsing_data_utils.h, and leave out values
// that are not available on Desktop.
export enum TimePeriodExperiment {
  NOT_SELECTED = -1,
  LAST_HOUR = 0,
  LAST_DAY = 1,
  LAST_WEEK = 2,
  FOUR_WEEKS = 3,
  ALL_TIME = 4,
  // OLDER_THAN_30_DAYS = 5 is not used on Desktop.
  LAST_15_MINUTES = 6,
  TIME_PERIOD_LAST = LAST_15_MINUTES
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

  recordSettingsClearBrowsingDataBasicTimePeriodHistogram(
      bucket: TimePeriodExperiment): void;

  recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram(
      bucket: TimePeriodExperiment): void;
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

  recordSettingsClearBrowsingDataBasicTimePeriodHistogram(
      bucket: TimePeriodExperiment) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.ClearBrowsingData.Basic.TimePeriod',
      bucket,
      TimePeriodExperiment.TIME_PERIOD_LAST,
    ]);
  }

  recordSettingsClearBrowsingDataAdvancedTimePeriodHistogram(
      bucket: TimePeriodExperiment) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.ClearBrowsingData.Advanced.TimePeriod',
      bucket,
      TimePeriodExperiment.TIME_PERIOD_LAST,
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
