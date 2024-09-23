// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'I18nBehavior' is a behavior to mix in loading of internationalization
 * strings. Typically it is used as [[i18n('someString')]] computed bindings or
 * for this.i18n('foo'). It is not needed for HTML $i18n{otherString}, which is
 * handled by a C++ templatizer.
 * NOTE: This file is deprecated in favor of i18n_mixin.ts. Don't use it in new
 * code.
 */

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';

import {parseHtmlSubset, sanitizeInnerHtml, SanitizeInnerHtmlOpts} from './parse_html_subset.js';

/** @polymerBehavior */
export const I18nBehavior = {
  // <if expr="chromeos_ash">
  // Dynamic locale changes are only relevant in the multidevice_setup step in
  // ChromeOS OOBE/Login flows. On other platforms Chrome process is restarted
  // upon locale changes.
  // TODO(b/328408932): Migrate multidevice_setup to use oobe_i18n_mixin
  properties: {
    /**
     * The locale the UI is presented in. Used to signal dynamic locale
     * change.
     */
    locale: {
      type: String,
      value: '',
    },
  },

  /**
   * Call this when UI strings may have changed. This will send an update to
   * any data bindings to i18nDynamic(locale, ...).
   * @suppress {checkTypes}
   */
  i18nUpdateLocale() {
    this.locale = loadTimeData.getString('app_locale');
  },
  // </if>

  /**
   * Returns a translated string where $1 to $9 are replaced by the given
   * values.
   * @param {string} id The ID of the string to translate.
   * @param {...string} varArgs Values to replace the placeholders $1 to $9
   *     in the string.
   * @return {string} A translated, substituted string.
   * @private
   */
  i18nRaw_(id, varArgs) {
    return arguments.length === 1 ?
        loadTimeData.getString(id) :
        loadTimeData.getStringF.apply(loadTimeData, arguments);
  },

  /**
   * Returns a translated string where $1 to $9 are replaced by the given
   * values. Also sanitizes the output to filter out dangerous HTML/JS.
   * Use with Polymer bindings that are *not* inner-h-t-m-l.
   * NOTE: This is not related to $i18n{foo} in HTML, see file overview.
   * @param {string} id The ID of the string to translate.
   * @param {...string|number} varArgs Values to replace the placeholders $1
   *     to $9 in the string.
   * @return {string} A translated, sanitized, substituted string.
   */
  i18n(id, varArgs) {
    const rawString = this.i18nRaw_.apply(this, arguments);
    return parseHtmlSubset('<b>' + rawString + '</b>').firstChild.textContent;
  },

  /**
   * Similar to 'i18n', returns a translated, sanitized, substituted string.
   * It receives the string ID and a dictionary containing the substitutions
   * as well as optional additional allowed tags and attributes. Use with
   * Polymer bindings that are inner-h-t-m-l, for example.
   * @param {string} id The ID of the string to translate.
   * @param {SanitizeInnerHtmlOpts=} opts
   * @return {TrustedHTML}
   */
  i18nAdvanced(id, opts) {
    opts = opts || {};
    const args = [id].concat(opts.substitutions || []);
    const rawString = this.i18nRaw_.apply(this, args);
    return sanitizeInnerHtml(rawString, opts);
  },

  /**
   * Similar to 'i18n', with an unused |locale| parameter used to trigger
   * updates when |this.locale| changes.
   * @param {string} locale The UI language used.
   * @param {string} id The ID of the string to translate.
   * @param {...string} varArgs Values to replace the placeholders $1 to $9
   *     in the string.
   * @return {string} A translated, sanitized, substituted string.
   */
  i18nDynamic(locale, id, varArgs) {
    return this.i18n.apply(this, Array.prototype.slice.call(arguments, 1));
  },

  /**
   * Similar to 'i18nDynamic', but varArgs valus are interpreted as keys in
   * loadTimeData. This allows generation of strings that take other localized
   * strings as parameters.
   * @param {string} locale The UI language used.
   * @param {string} id The ID of the string to translate.
   * @param {...string} varArgs Values to replace the placeholders $1 to $9
   *     in the string. Values are interpreted as strings IDs if found in the
   *     list of localized strings.
   * @return {string} A translated, sanitized, substituted string.
   */
  i18nRecursive(locale, id, varArgs) {
    let args = Array.prototype.slice.call(arguments, 2);
    if (args.length > 0) {
      // Try to replace IDs with localized values.
      const self = this;
      args = args.map(function(str) {
        return self.i18nExists(str) ? loadTimeData.getString(str) : str;
      });
    }
    return this.i18nDynamic.apply(this, [locale, id].concat(args));
  },

  /**
   * Returns true if a translation exists for |id|.
   * @param {string} id
   * @return {boolean}
   */
  i18nExists(id) {
    return loadTimeData.valueExists(id);
  },
};

/** @interface */
export class I18nBehaviorInterface {
  constructor() {
    // <if expr="chromeos_ash">
    /** @type {string} */
    this.locale;
    // </if>
  }

  // <if expr="chromeos_ash">
  i18nUpdateLocale() {}
  // </if>

  /**
   * @param {string} id
   * @param {...string|number} varArgs
   * @return {string}
   */
  i18n(id, varArgs) {}

  /**
   * @param {string} id
   * @param {SanitizeInnerHtmlOpts=} opts
   * @return {string}
   */
  i18nAdvanced(id, opts) {}

  /**
   * @param {string} locale
   * @param {string} id
   * @param {...string} varArgs
   * @return {string}
   */
  i18nDynamic(locale, id, varArgs) {}

  /**
   * @param {string} locale
   * @param {string} id
   * @param {...string} varArgs
   * @return {string}
   */
  i18nRecursive(locale, id, varArgs) {}

  /**
   * @param {string} id
   * @return {boolean}
   */
  i18nExists(id) {}
}
