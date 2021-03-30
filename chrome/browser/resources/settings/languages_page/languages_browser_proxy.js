// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the languages section
 * to interact with the browser.
 */

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

  /** @interface */
export class LanguagesBrowserProxy {
  // <if expr="chromeos or is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't
   * affect the actual UI language until a restart.
   * @param {string} languageCode
   */
  setProspectiveUILanguage(languageCode) {}

  /** @return {!Promise<string>} */
  getProspectiveUILanguage() {}

  // </if>

  /** @return {!LanguageSettingsPrivate} */
  getLanguageSettingsPrivate() {}

  // <if expr="chromeos">
  /** @return {!InputMethodPrivate} */
  getInputMethodPrivate() {}
  // </if>
}

/**
 * @implements {LanguagesBrowserProxy}
 */
export class LanguagesBrowserProxyImpl {
  // <if expr="chromeos or is_win">
  /** @override */
  setProspectiveUILanguage(languageCode) {
    chrome.send('setProspectiveUILanguage', [languageCode]);
  }

  /** @override */
  getProspectiveUILanguage() {
    return sendWithPromise('getProspectiveUILanguage');
  }

  // </if>

  /** @override */
  getLanguageSettingsPrivate() {
    return /** @type {!LanguageSettingsPrivate} */ (
        chrome.languageSettingsPrivate);
  }

  // <if expr="chromeos">
  /** @override */
  getInputMethodPrivate() {
    return /** @type {!InputMethodPrivate} */ (chrome.inputMethodPrivate);
  }
  // </if>
}

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
addSingletonGetter(LanguagesBrowserProxyImpl);
