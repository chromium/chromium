// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview
 * Handles metrics for ChromeOS's languages OS settings.
 * TODO(crbug/1109431): Remove these metrics when languages settings migration
 * is completed and data analysed.
 */
cr.define('settings', function() {
  /**
   * Keeps in sync with SettingsLanguagesPageInteraction
   * in tools/metrics/histograms/enums.xml.
   * @enum {number}
   */
  /* #export */ const LanguagesPageInteraction = {
    SWITCH_SYSTEM_LANGUAGE: 0,
    RESTART: 1,
    SWITCH_INPUT_METHOD: 2,
    RESTART_LATER: 3,
    OPEN_CUSTOM_SPELL_CHECK: 4,
  };

  /** @interface */
  /* #export */ class LanguagesMetricsProxy {
    /**
     * Records the interaction to enumerated histogram.
     * @param {!settings.LanguagesPageInteraction} interaction
     */
    recordInteraction(interaction) {}

    /** Records when users select "Add input method". */
    recordAddInputMethod() {}

    /** Records when users select "Add languages". */
    recordAddLanguages() {}

    /** Records when users select "Manage input methods". */
    recordManageInputMethods() {}

    /**
     * Records when users toggle "Show Input Options On Shelf" option.
     * @param {boolean} value
     */
    recordToggleShowInputOptionsOnShelf(value) {}

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

  /** @implements {settings.LanguagesMetricsProxy} */
  /* #export */ class LanguagesMetricsProxyImpl {
    /** @override */
    recordInteraction(interaction) {
      chrome.metricsPrivate.recordEnumerationValue(
          'ChromeOS.Settings.Languages.Interaction', interaction,
          Object.keys(LanguagesPageInteraction).length);
    }

    /** @override */
    recordAddInputMethod() {
      chrome.metricsPrivate.recordUserAction(
          'ChromeOS.Settings.Languages.AddInputMethod');
    }

    /** @override */
    recordAddLanguages() {
      chrome.metricsPrivate.recordUserAction(
          'ChromeOS.Settings.Languages.AddLanguages');
    }

    /** @override */
    recordManageInputMethods() {
      chrome.metricsPrivate.recordUserAction(
          'ChromeOS.Settings.Languages.ManageInputMethods');
    }

    /** @override */
    recordToggleShowInputOptionsOnShelf(value) {
      chrome.metricsPrivate.recordBoolean(
          'ChromeOS.Settings.Languages.Toggle.ShowInputOptionsOnShelf', value);
    }

    /** @override */
    recordToggleSpellCheck(value) {
      chrome.metricsPrivate.recordBoolean(
          'ChromeOS.Settings.Languages.Toggle.SpellCheck', value);
    }

    /** @override */
    recordToggleTranslate(value) {
      chrome.metricsPrivate.recordBoolean(
          'ChromeOS.Settings.Languages.Toggle.Translate', value);
    }

    /** @override */
    recordTranslateCheckboxChanged(value) {
      chrome.metricsPrivate.recordBoolean(
          'ChromeOS.Settings.Languages.OfferToTranslateCheckbox', value);
    }
  }

  cr.addSingletonGetter(LanguagesMetricsProxyImpl);

  // #cr_define_end
  return {
    LanguagesMetricsProxy,
    LanguagesMetricsProxyImpl,
    LanguagesPageInteraction,
  };
});
