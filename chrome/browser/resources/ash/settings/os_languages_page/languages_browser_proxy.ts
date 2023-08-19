// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the languages section
 * to interact with the browser.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface LanguagesBrowserProxy {
  /**
   * Sets the prospective UI language to the chosen language. This won't
   * affect the actual UI language until a restart.
   */
  setProspectiveUiLanguage(languageCode: string): void;

  getProspectiveUiLanguage(): Promise<string>;

  getLanguageSettingsPrivate(): typeof chrome.languageSettingsPrivate;

  getInputMethodPrivate(): typeof chrome.inputMethodPrivate;
}

let instance: LanguagesBrowserProxy|null = null;

export class LanguagesBrowserProxyImpl implements LanguagesBrowserProxy {
  static getInstance(): LanguagesBrowserProxy {
    return instance || (instance = new LanguagesBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: LanguagesBrowserProxy): void {
    instance = obj;
  }

  setProspectiveUiLanguage(languageCode: string): void {
    chrome.send('setProspectiveUILanguage', [languageCode]);
  }

  getProspectiveUiLanguage(): Promise<string> {
    return sendWithPromise('getProspectiveUILanguage');
  }

  getLanguageSettingsPrivate(): typeof chrome.languageSettingsPrivate {
    return chrome.languageSettingsPrivate;
  }

  getInputMethodPrivate(): typeof chrome.inputMethodPrivate {
    return chrome.inputMethodPrivate;
  }
}
