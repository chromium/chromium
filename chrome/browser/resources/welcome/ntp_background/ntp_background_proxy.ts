// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {NtpBackgroundMetricsProxyImpl} from './ntp_background_metrics_proxy.js';

export type NtpBackgroundData = {
  id: number,
  imageUrl: string,
  thumbnailClass: string,
  title: string
};

export interface NtpBackgroundProxy {
  clearBackground(): void;
  getBackgrounds(): Promise<NtpBackgroundData[]>;
  preloadImage(url: string): Promise<void>;
  recordBackgroundImageFailedToLoad(): void;
  recordBackgroundImageLoadTime(loadTime: number): void;
  recordBackgroundImageNeverLoaded(): void;
  setBackground(id: number): void;
}

export class NtpBackgroundProxyImpl implements NtpBackgroundProxy {
  clearBackground() {
    return chrome.send('clearBackground');
  }

  getBackgrounds() {
    return sendWithPromise('getBackgrounds');
  }

  preloadImage(url: string) {
    return new Promise((resolve, reject) => {
             const preloadedImage = new Image();
             preloadedImage.onerror = reject;
             preloadedImage.onload = () => resolve();
             preloadedImage.src = url;
           }) as Promise<void>;
  }

  recordBackgroundImageFailedToLoad() {
    const ntpInteractions =
        NtpBackgroundMetricsProxyImpl.getInstance().getInteractions();
    chrome.metricsPrivate.recordEnumerationValue(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        ntpInteractions.BackgroundImageFailedToLoad,
        Object.keys(ntpInteractions).length);
  }

  recordBackgroundImageLoadTime(loadTime: number) {
    chrome.metricsPrivate.recordTime(
        'FirstRun.NewUserExperience.NtpBackgroundLoadTime', loadTime);
  }

  recordBackgroundImageNeverLoaded() {
    const ntpInteractions =
        NtpBackgroundMetricsProxyImpl.getInstance().getInteractions();
    chrome.metricsPrivate.recordEnumerationValue(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        ntpInteractions.BackgroundImageNeverLoaded,
        Object.keys(ntpInteractions).length);
  }

  setBackground(id: number) {
    chrome.send('setBackground', [id]);
  }

  static getInstance(): NtpBackgroundProxy {
    return instance || (instance = new NtpBackgroundProxyImpl());
  }

  static setInstance(obj: NtpBackgroundProxy) {
    instance = obj;
  }
}

let instance: NtpBackgroundProxy|null = null;
