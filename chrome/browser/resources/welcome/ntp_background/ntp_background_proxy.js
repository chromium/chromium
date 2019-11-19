// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {NtpBackgroundMetricsProxyImpl} from './ntp_background_metrics_proxy.js';

/**
 * @typedef {{
 *   id: number,
 *   imageUrl: string,
 *   thumbnailClass: string,
 *   title: string,
 * }}
 */
export let NtpBackgroundData;

/** @interface */
export class NtpBackgroundProxy {
  /** @return {!Promise} */
  clearBackground() {}

  /** @return {!Promise<!Array<!NtpBackgroundData>>} */
  getBackgrounds() {}

  /**
   * @param {string} url
   * @return {!Promise<void>}
   */
  preloadImage(url) {}

  recordBackgroundImageFailedToLoad() {}

  /** @param {number} loadTime */
  recordBackgroundImageLoadTime(loadTime) {}

  recordBackgroundImageNeverLoaded() {}

  /** @param {number} id */
  setBackground(id) {}
}

/** @implements {NtpBackgroundProxy} */
export class NtpBackgroundProxyImpl {
  /** @override */
  clearBackground() {
    return sendWithPromise('clearBackground');
  }

  /** @override */
  getBackgrounds() {
    return sendWithPromise('getBackgrounds');
  }

  /** @override */
  preloadImage(url) {
    return new Promise((resolve, reject) => {
      const preloadedImage = new Image();
      preloadedImage.onerror = reject;
      preloadedImage.onload = resolve;
      preloadedImage.src = url;
    });
  }

  /** @override */
  recordBackgroundImageFailedToLoad() {
    const ntpInteractions =
        NtpBackgroundMetricsProxyImpl.getInstance().getInteractions();
    chrome.metricsPrivate.recordEnumerationValue(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        ntpInteractions.BackgroundImageFailedToLoad,
        Object.keys(ntpInteractions).length);
  }

  /** @override */
  recordBackgroundImageLoadTime(loadTime) {
    chrome.metricsPrivate.recordTime(
        'FirstRun.NewUserExperience.NtpBackgroundLoadTime', loadTime);
  }

  /** @override */
  recordBackgroundImageNeverLoaded() {
    const ntpInteractions =
        NtpBackgroundMetricsProxyImpl.getInstance().getInteractions();
    chrome.metricsPrivate.recordEnumerationValue(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        ntpInteractions.BackgroundImageNeverLoaded,
        Object.keys(ntpInteractions).length);
  }

  /** @override */
  setBackground(id) {
    chrome.send('setBackground', [id]);
  }
}

addSingletonGetter(NtpBackgroundProxyImpl);
