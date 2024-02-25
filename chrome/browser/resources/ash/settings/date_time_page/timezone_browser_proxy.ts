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

  static setInstanceForTesting(obj: TimeZoneBrowserProxy): void {
    instance = obj;
  }

  showParentAccessForTimeZone(): void {
    chrome.send('handleShowParentAccessForTimeZone');
  }

  dateTimePageReady(): void {
    chrome.send('dateTimePageReady');
  }

  showSetDateTimeUi(): void {
    chrome.send('showSetDateTimeUI');
  }

  getTimeZones(): Promise<string[][]> {
    return sendWithPromise('getTimeZones');
  }
}
