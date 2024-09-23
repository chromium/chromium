// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {NuxNtpBackgroundInteractions} from '../shared/module_metrics_proxy.js';

export interface NtpBackgroundData {
  id: number;
  imageUrl: string;
  thumbnailClass: string;
  title: string;
}

export interface NtpBackgroundProxy {
  clearBackground(): void;
  getBackgrounds(): Promise<NtpBackgroundData[]>;
  preloadImage(url: string): Promise<void>;
  recordBackgroundImageFailedToLoad(): void;
  recordBackgroundImageNeverLoaded(): void;
  setBackground(id: number): void;
}

export class NtpBackgroundProxyImpl implements NtpBackgroundProxy {
  clearBackground() {
    chrome.send('clearBackground');
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
        NuxNtpBackgroundInteractions.BACKGROUND_IMAGE_FAILED_TO_LOAD,
        Object.keys(NuxNtpBackgroundInteractions).length);
  }

  recordBackgroundImageNeverLoaded() {
    chrome.metricsPrivate.recordEnumerationValue(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        NuxNtpBackgroundInteractions.BACKGROUND_IMAGE_NEVER_LOADED,
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
