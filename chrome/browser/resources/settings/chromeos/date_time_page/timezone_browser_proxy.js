// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @fileoverview A helper object used by the time zone subpage page. */
  /** @interface */
export class TimeZoneBrowserProxy {
  /** Notifies C++ code to show parent access code verification view. */
  showParentAccessForTimeZone() {}

  /** Notifies C++ code that the date_time page is ready. */
  dateTimePageReady() {}

  /** Notifies C++ code to show the chrome://set-time standalone dialog. */
  showSetDateTimeUI() {}

  /** @return {!Promise<!Array<!Array<string>>>} */
  getTimeZones() {}
}

/** @implements {TimeZoneBrowserProxy} */
export class TimeZoneBrowserProxyImpl {
  /** @override */
  showParentAccessForTimeZone() {
    chrome.send('handleShowParentAccessForTimeZone');
  }

  /** @override */
  dateTimePageReady() {
    chrome.send('dateTimePageReady');
  }

  /** @override */
  showSetDateTimeUI() {
    chrome.send('showSetDateTimeUI');
  }

  /** @override */
  getTimeZones() {
    return sendWithPromise('getTimeZones');
  }
}

addSingletonGetter(TimeZoneBrowserProxyImpl);
