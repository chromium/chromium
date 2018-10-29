// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communication for the privacy page. */

/** @typedef {{enabled: boolean, managed: boolean}} */
let MetricsReporting;

cr.define('settings', function() {
  /** @interface */
  class PrivacyPageBrowserProxy {
    // <if expr="_google_chrome and not chromeos">
    /** @return {!Promise<!MetricsReporting>} */
    getMetricsReporting() {}

    /** @param {boolean} enabled */
    setMetricsReportingEnabled(enabled) {}

    // </if>

    // <if expr="is_win or is_macosx">
    /** Invokes the native certificate manager (used by win and mac). */
    showManageSSLCertificates() {}

    // </if>

    /** @param {boolean} enabled */
    setBlockAutoplayEnabled(enabled) {}
  }

  /**
   * @implements {settings.PrivacyPageBrowserProxy}
   */
  class PrivacyPageBrowserProxyImpl {
    // <if expr="_google_chrome and not chromeos">
    /** @override */
    getMetricsReporting() {
      return cr.sendWithPromise('getMetricsReporting');
    }

    /** @override */
    setMetricsReportingEnabled(enabled) {
      chrome.send('setMetricsReportingEnabled', [enabled]);
    }

    // </if>

    /** @override */
    setBlockAutoplayEnabled(enabled) {
      chrome.send('setBlockAutoplayEnabled', [enabled]);
    }

    // <if expr="is_win or is_macosx">
    /** @override */
    showManageSSLCertificates() {
      chrome.send('showManageSSLCertificates');
    }
    // </if>
  }

  cr.addSingletonGetter(PrivacyPageBrowserProxyImpl);

  return {
    PrivacyPageBrowserProxy: PrivacyPageBrowserProxy,
    PrivacyPageBrowserProxyImpl: PrivacyPageBrowserProxyImpl,
  };
});
