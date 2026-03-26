// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communication for the privacy page. */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface MetricsReporting {
  enabled: boolean;
  managed: boolean;
}

export interface PrivacyPageBrowserProxy {
  // <if expr="_google_chrome and not is_chromeos">
  getMetricsReporting(): Promise<MetricsReporting>;
  setMetricsReportingEnabled(enabled: boolean): void;
  // </if>
}

export class PrivacyPageBrowserProxyImpl implements PrivacyPageBrowserProxy {
  // <if expr="_google_chrome and not is_chromeos">
  getMetricsReporting() {
    return sendWithPromise<MetricsReporting>('getMetricsReporting');
  }

  setMetricsReportingEnabled(enabled: boolean) {
    chrome.send('setMetricsReportingEnabled', [enabled]);
  }
  // </if>

  static getInstance(): PrivacyPageBrowserProxy {
    return instance || (instance = new PrivacyPageBrowserProxyImpl());
  }

  static setInstance(obj: PrivacyPageBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacyPageBrowserProxy|null = null;
