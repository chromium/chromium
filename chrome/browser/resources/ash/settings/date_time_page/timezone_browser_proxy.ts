// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used by the time zone subpage page. */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface TimeZoneBrowserProxy {
  /** Notifies C++ code to show parent access code verification view. */
  showParentAccessForTimeZone(): void;

  /** Notifies C++ code that the date_time page is ready. */
  dateTimePageReady(): void;

  /** Notifies C++ code to show the chrome://set-time standalone dialog. */
  showSetDateTimeUi(): void;

  getTimeZones(): Promise<string[][]>;
}

let instance: TimeZoneBrowserProxy|null = null;

export class TimeZoneBrowserProxyImpl implements TimeZoneBrowserProxy {
  static getInstance(): TimeZoneBrowserProxy {
    return instance || (instance = new TimeZoneBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: TimeZoneBrowserProxy) {
    instance = obj;
  }

  showParentAccessForTimeZone() {
    chrome.send('handleShowParentAccessForTimeZone');
  }

  dateTimePageReady() {
    chrome.send('dateTimePageReady');
  }

  showSetDateTimeUi() {
    chrome.send('showSetDateTimeUI');
  }

  getTimeZones() {
    return sendWithPromise('getTimeZones');
  }
}
