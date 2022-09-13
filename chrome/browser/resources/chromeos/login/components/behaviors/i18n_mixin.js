// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 */
// #import { dedupingMixin } from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import { loadTimeData } from 'chrome://resources/js/load_time_data.m.js';
// #import {parseHtmlSubset, SanitizeInnerHtmlOpts, sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

/**
 * TODO(crbug.com/1319543) - Remove this file once it is possible.
 *
 * This file is essentially a copy of the JavaScript file that is generated
 * from ui/webui/resources/js/i18n_mixin.ts. The generated file cannot be used
 * in OOBE because it always has JS imports directives. For the Polymer2 OOBE,
 * the directives need to be commented out.
 *
 */

/**
 * @polymer
 * @mixinFunction
 */
/* #export */ const I18nMixin = Polymer.dedupingMixin((superClass) => {
  /**
   * @polymer
   * @mixinClass
   */
  class I18nMixin extends superClass {
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
    }

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
    }

    /**
     * Similar to 'i18n', returns a translated, sanitized, substituted string.
     * It receives the string ID and a dictionary containing the substitutions
     * as well as optional additional allowed tags and attributes. Use with
     * Polymer bindings that are inner-h-t-m-l, for example.
     * @param {string} id The ID of the string to translate.
     * @param {SanitizeInnerHtmlOpts=} opts
     * @return {string}
     */
    i18nAdvanced(id, opts) {
      opts = opts || {};
      const args = [id].concat(opts.substitutions || []);
      const rawString = this.i18nRaw_.apply(this, args);
      return sanitizeInnerHtml(rawString, opts);
    }

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
    }

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
    }

    /**
     * Returns true if a translation exists for |id|.
     * @param {string} id
     * @return {boolean}
     */
    i18nExists(id) {
      return loadTimeData.valueExists(id);
    }
  }
  return I18nMixin;
});