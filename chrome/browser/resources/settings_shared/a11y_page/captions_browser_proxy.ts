// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the Chrome captions section to
 * interact with the browser. Used on operating system that is not Chrome OS.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * |name| is the display name of a language, ex. German.
 * |code| is the language code, ex. de-DE.
 * |downloadProgress| is the display-friendly download progress as the language
 *     model is being downloaded.
 */
export interface LiveCaptionLanguage {
  displayName: string;
  nativeDisplayName: string;
  code: string;
  downloadProgress: string;
}

export type LiveCaptionLanguageList = LiveCaptionLanguage[];

export interface CaptionsBrowserProxy {
  /**
   * Open the native captions system dialog.
   */
  openSystemCaptionsDialog(): void;

  liveCaptionSectionReady(): void;

  getInstalledLanguagePacks(): Promise<LiveCaptionLanguageList>;

  getAvailableLanguagePacks(): Promise<LiveCaptionLanguageList>;

  removeLanguagePack(languageCode: string): void;

  installLanguagePacks(languageCodes: string[]): void;
}

export class CaptionsBrowserProxyImpl implements CaptionsBrowserProxy {
  openSystemCaptionsDialog() {
    chrome.send('openSystemCaptionsDialog');
  }

  liveCaptionSectionReady() {
    chrome.send('liveCaptionSectionReady');
  }

  getInstalledLanguagePacks() {
    return sendWithPromise('getInstalledLanguagePacks');
  }

  getAvailableLanguagePacks() {
    return sendWithPromise('getAvailableLanguagePacks');
  }

  removeLanguagePack(languageCode: string) {
    chrome.send('removeLanguagePack', [languageCode]);
  }

  installLanguagePacks(languageCodes: string[]) {
    chrome.send('installLanguagePacks', languageCodes);
  }

  static getInstance(): CaptionsBrowserProxy {
    return instance || (instance = new CaptionsBrowserProxyImpl());
  }

  static setInstance(obj: CaptionsBrowserProxy) {
    instance = obj;
  }
}

let instance: CaptionsBrowserProxy|null = null;
