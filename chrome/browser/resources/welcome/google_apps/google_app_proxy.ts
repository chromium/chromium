// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {BookmarkListItem} from '../shared/nux_types.js';

enum NuxGoogleAppsSelections {
  GMAIL_DEPRECATED = 0,
  YOU_TUBE,
  MAPS,
  TRANSLATE,
  NEWS,
  CHROME_WEB_STORE,
}

export interface GoogleAppProxy {
  /**
   * Google app IDs are local to the list of Google apps, so their icon must
   * be cached by the handler that provided the IDs.
   */
  cacheBookmarkIcon(appId: number): void;

  /**
   * Returns a promise for an array of Google apps.
   */
  getAppList(): Promise<BookmarkListItem[]>;

  /**
   * @param providerId This should match one of the histogram enum
   *     value for NuxGoogleAppsSelections.
   */
  recordProviderSelected(providerId: number): void;
}

export class GoogleAppProxyImpl implements GoogleAppProxy {
  cacheBookmarkIcon(appId: number) {
    chrome.send('cacheGoogleAppIcon', [appId]);
  }

  getAppList() {
    return sendWithPromise('getGoogleAppsList');
  }

  recordProviderSelected(providerId: number) {
    chrome.metricsPrivate.recordEnumerationValue(
        'FirstRun.NewUserExperience.GoogleAppsSelection', providerId,
        Object.keys(NuxGoogleAppsSelections).length);
  }

  static getInstance(): GoogleAppProxy {
    return instance || (instance = new GoogleAppProxyImpl());
  }

  static setInstance(obj: GoogleAppProxy) {
    instance = obj;
  }
}

let instance: GoogleAppProxy|null = null;
