// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixin, I18nMixinInterface} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {SanitizeInnerHtmlOpts} from '//resources/ash/common/parse_html_subset.js';

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'OobeI18nMixin' is extended I18nMixin with automatic locale change
 * propagation for children.
 */

type Constructor<T> = new (...args: any[]) => T;

export const OobeI18nMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<OobeI18nMixinInterface> => {
      const superClassBase = I18nMixin(superClass);
      class OobeI18nMixinInternal extends superClassBase implements
          OobeI18nMixinInterface {
        static get properties(): PolymerElementProperties {
          return {
            locale: {
              type: String,
              value: '',
            },
          };
        }

        private locale: string;

        override ready() {
          super.ready();
          this.classList.add('i18n-dynamic');
        }

        /**
         * Similar to 'i18nAdvanced', with an unused |locale| parameter used to
         * trigger updates when |this.locale| changes.
         * @param locale The UI language used.
         * @param id The ID of the string to translate.
         * @return A translated, sanitized, substituted string.
         */
        i18nAdvancedDynamic(_locale: string, id: string,
            opts?: SanitizeInnerHtmlOpts): TrustedHTML {
          return this.i18nAdvanced(id, opts);
        }

        i18nUpdateLocale(): void {
          this.locale = loadTimeData.getString('app_locale');
          const matches = this.shadowRoot?.querySelectorAll('.i18n-dynamic');
          for (const child of matches || []) {
            if ('i18nUpdateLocale' in child &&
                typeof (child.i18nUpdateLocale) === 'function') {
              child.i18nUpdateLocale();
            }
          }
        }
      }

      return OobeI18nMixinInternal;
    });

export interface OobeI18nMixinInterface extends I18nMixinInterface {
  i18nUpdateLocale(): void;
  /**
   * @param locale The UI language used.
   * @param id The ID of the string to translate.
   * @return A translated, sanitized, substituted string.
   */
  i18nAdvancedDynamic(locale: string, id: string,
    opts?: SanitizeInnerHtmlOpts): TrustedHTML;
}
