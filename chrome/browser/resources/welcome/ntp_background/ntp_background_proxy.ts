// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {NuxNtpBackgroundInteractions} from '../shared/module_metrics_proxy.js';

export type NtpBackgroundData = {
  id: number,
  imageUrl: string,
  thumbnailClass: string,
  title: string,
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
    chrome.metricsPrivate.recordEnumerationValue(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        NuxNtpBackgroundInteractions.BackgroundImageFailedToLoad,
        Object.keys(NuxNtpBackgroundInteractions).length);
  }

  recordBackgroundImageLoadTime(loadTime: number) {
    chrome.metricsPrivate.recordTime(
        'FirstRun.NewUserExperience.NtpBackgroundLoadTime', loadTime);
  }

  recordBackgroundImageNeverLoaded() {
    chrome.metricsPrivate.recordEnumerationValue(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        NuxNtpBackgroundInteractions.BackgroundImageNeverLoaded,
        Object.keys(NuxNtpBackgroundInteractions).length);
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
