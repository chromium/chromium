// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'I18nMixin' is a Mixin offering loading of internationalization
 * strings. Typically it is used as [[i18n('someString')]] computed bindings or
 * for this.i18n('foo'). It is not needed for HTML $i18n{otherString}, which is
 * handled by a C++ templatizer.
 *
 * Forked from ui/webui/resources/cr_elements/i18n_mixin.ts
 */

import {loadTimeData} from '//resources/js/load_time_data.js';
import {parseHtmlSubset, sanitizeInnerHtml, SanitizeInnerHtmlOpts} from '//resources/js/parse_html_subset.js';
import {dedupingMixin, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export const I18nMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<I18nMixinInterface> => {
      class I18nMixin extends superClass implements I18nMixinInterface {
        /**
         * Returns a translated string where $1 to $9 are replaced by the given
         * values.
         * @param id The ID of the string to translate.
         * @param varArgs Values to replace the placeholders $1 to $9 in the
         *     string.
         * @return A translated, substituted string.
         */
        private i18nRaw_(id: string, ...varArgs: Array<string|number>) {
          return varArgs.length === 0 ? loadTimeData.getString(id) :
                                        loadTimeData.getStringF(id, ...varArgs);
        }

        /**
         * Returns a translated string where $1 to $9 are replaced by the given
         * values. Also sanitizes the output to filter out dangerous HTML/JS.
         * Use with Polymer bindings that are *not* inner-h-t-m-l.
         * NOTE: This is not related to $i18n{foo} in HTML, see file overview.
         * @param id The ID of the string to translate.
         * @param varArgs Values to replace the placeholders $1 to $9 in the
         *     string.
         * @return A translated, sanitized, substituted string.
         */
        i18n(id: string, ...varArgs: Array<string|number>) {
          const rawString = this.i18nRaw_(id, ...varArgs);
          return parseHtmlSubset(`<b>${rawString}</b>`).firstChild!.textContent!
              ;
        }

        /**
         * Similar to 'i18n', returns a translated, sanitized, substituted
         * string. It receives the string ID and a dictionary containing the
         * substitutions as well as optional additional allowed tags and
         * attributes. Use with Polymer bindings that are inner-h-t-m-l, for
         * example.
         * @param id The ID of the string to translate.
         */
        i18nAdvanced(id: string, opts?: SanitizeInnerHtmlOpts) {
          opts = opts || {};
          const rawString = this.i18nRaw_(id, ...(opts.substitutions || []));
          return sanitizeInnerHtml(rawString, opts);
        }

        /**
         * Similar to 'i18n', with an unused |locale| parameter used to trigger
         * updates when the locale changes.
         * @param locale The UI language used.
         * @param id The ID of the string to translate.
         * @param varArgs Values to replace the placeholders $1 to $9 in the
         *     string.
         * @return A translated, sanitized, substituted string.
         */
        i18nDynamic(_locale: string, id: string, ...varArgs: string[]) {
          return this.i18n(id, ...varArgs);
        }

        /**
         * Similar to 'i18nDynamic', but varArgs valus are interpreted as keys
         * in loadTimeData. This allows generation of strings that take other
         * localized strings as parameters.
         * @param locale The UI language used.
         * @param id The ID of the string to translate.
         * @param varArgs Values to replace the placeholders $1 to $9
         *     in the string. Values are interpreted as strings IDs if found in
         * the list of localized strings.
         * @return A translated, sanitized, substituted string.
         */
        i18nRecursive(locale: string, id: string, ...varArgs: string[]) {
          let args = varArgs;
          if (args.length > 0) {
            // Try to replace IDs with localized values.
            args = args.map(str => {
              return this.i18nExists(str) ? loadTimeData.getString(str) : str;
            });
          }
          return this.i18nDynamic(locale, id, ...args);
        }

        /**
         * Returns true if a translation exists for |id|.
         */
        i18nExists(id: string) {
          return loadTimeData.valueExists(id);
        }
      }

      return I18nMixin;
    });

export interface I18nMixinInterface {
  i18n(id: string, ...varArgs: Array<string|number>): string;
  i18nAdvanced(id: string, opts?: SanitizeInnerHtmlOpts): TrustedHTML;
  i18nDynamic(locale: string, id: string, ...varArgs: string[]): string;
  i18nRecursive(locale: string, id: string, ...varArgs: string[]): string;
  i18nExists(id: string): boolean;
}
