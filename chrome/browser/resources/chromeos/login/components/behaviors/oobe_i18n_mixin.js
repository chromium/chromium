// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'OobeI18nMixin' is a Mixin that provides dynamic content reloading  when
 * locale changes. See I18nMixin for more details.
 */
// #import { dedupingMixin } from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import { loadTimeData } from 'chrome://resources/js/load_time_data.m.js';
// #import { SanitizeInnerHtmlOpts } from 'chrome://resources/js/parse_html_subset.js';
// #import { I18nMixin } from './i18n_mixin.m.js';

/**
 * @polymer
 * @mixinFunction
 */
/* #export */ const OobeI18nMixin = Polymer.dedupingMixin((superClass) => {
  /**
   * @polymer
   * @mixinClass
   */
  class OobeI18nMixin extends I18nMixin(superClass) {
    ready() {
      super.ready();
      this.classList.add('i18n-dynamic');
    }

    /**
     * Similar to 'i18nAdvanced', with an unused |locale| parameter used to
     * trigger updates when |this.locale| changes.
     * @param {string} locale The UI language used.
     * @param {string} id The ID of the string to translate.
     * @param {SanitizeInnerHtmlOpts=} opts
     * @return {string} A translated, sanitized, substituted string.
     */
    i18nAdvancedDynamic(locale, id, opts) {
      return this.i18nAdvanced(id, opts);
    }

    i18nUpdateLocale() {
      this.locale = loadTimeData.getString('app_locale');
      const matches = this.shadowRoot.querySelectorAll('.i18n-dynamic');
      for (const child of matches) {
        if (typeof (child.i18nUpdateLocale) === 'function') {
          child.i18nUpdateLocale();
        }
      }
    }
  }
  return OobeI18nMixin;
});