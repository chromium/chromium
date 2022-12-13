// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the languages section
 * to interact with the browser.
 */

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

/** @interface */
export class LanguagesBrowserProxy {
  /**
   * Sets the prospective UI language to the chosen language. This won't
   * affect the actual UI language until a restart.
   * @param {string} languageCode
   */
  setProspectiveUiLanguage(languageCode) {}

  /** @return {!Promise<string>} */
  getProspectiveUiLanguage() {}

  /** @return {!LanguageSettingsPrivate} */
  getLanguageSettingsPrivate() {}

  /** @return {!InputMethodPrivate} */
  getInputMethodPrivate() {}
}

/** @type {?LanguagesBrowserProxy} */
let instance = null;

/**
 * @implements {LanguagesBrowserProxy}
 */
export class LanguagesBrowserProxyImpl {
  /** @return {!LanguagesBrowserProxy} */
  static getInstance() {
    return instance || (instance = new LanguagesBrowserProxyImpl());
  }

  /** @param {!LanguagesBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  setProspectiveUiLanguage(languageCode) {
    chrome.send('setProspectiveUILanguage', [languageCode]);
  }

  /** @override */
  getProspectiveUiLanguage() {
    return sendWithPromise('getProspectiveUILanguage');
  }

  /** @override */
  getLanguageSettingsPrivate() {
    return /** @type {!LanguageSettingsPrivate} */ (
        chrome.languageSettingsPrivate);
  }

  /** @override */
  getInputMethodPrivate() {
    return /** @type {!InputMethodPrivate} */ (chrome.inputMethodPrivate);
  }
}
