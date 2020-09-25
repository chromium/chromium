// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Handles metrics for ChromeOS's languages settings in the browser.
 * TODO(crbug/1109431): Remove these metrics when languages settings migration
 * is completed and data analysed.
 */

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * Keeps in sync with SettingsLanguagesPageBrowserInteraction
 * in tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const LanguagesPageInteraction = {
  SWITCH_SYSTEM_LANGUAGE: 0,
  RESTART: 1,
  OPEN_CUSTOM_SPELL_CHECK: 2,
};

/** @interface */
export class LanguagesMetricsProxy {
  /**
   * Records the interaction to enumerated histogram.
   * @param {!LanguagesPageInteraction} interaction
   */
  recordInteraction(interaction) {}

  /* Records when users select "Add languages". */
  recordAddLanguages() {}

  /**
   * Records when users toggle "Spell check" option.
   * @param {boolean} value
   */
  recordToggleSpellCheck(value) {}

  /**
   * Records when users toggle "Offer to translate languages you don't read"
   * option.
   * @param {boolean} value
   */
  recordToggleTranslate(value) {}

  /**
   * Records when users check/uncheck "Offer to translate pages in this
   * language" checkbox.
   * @param {boolean} value
   */
  recordTranslateCheckboxChanged(value) {}
}

/** @implements {LanguagesMetricsProxy} */
export class LanguagesMetricsProxyImpl {
  /** @override */
  recordInteraction(interaction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.Settings.Languages.Browser.Interaction', interaction,
        Object.keys(LanguagesPageInteraction).length);
  }

  /** @override */
  recordAddLanguages() {
    chrome.metricsPrivate.recordUserAction(
        'ChromeOS.Settings.Languages.Browser.AddLanguages');
  }

  /** @override */
  recordToggleSpellCheck(value) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Browser.Toggle.SpellCheck', value);
  }

  /** @override */
  recordToggleTranslate(value) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Browser.Toggle.Translate', value);
  }

  /** @override */
  recordTranslateCheckboxChanged(value) {
    chrome.metricsPrivate.recordBoolean(
        'ChromeOS.Settings.Languages.Browser.OfferToTranslateCheckbox', value);
  }
}

addSingletonGetter(LanguagesMetricsProxyImpl);
