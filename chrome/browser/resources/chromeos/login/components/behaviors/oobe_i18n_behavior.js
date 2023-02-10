// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {SanitizeInnerHtmlOpts} from '//resources/ash/common/parse_html_subset.js';
// clang-format on

/**
 * @fileoverview
 * 'OobeI18nBehavior' is extended I18nBehavior with automatic locale change
 * propagation for children.
 */

/** @polymerBehavior */
const OobeI18nBehaviorImpl = {
  ready() {
    this.classList.add('i18n-dynamic');
  },

  /**
   * Similar to 'i18nAdvanced', with an unused |locale| parameter used to
   * trigger updates when |this.locale| changes.
   * @param {string} locale The UI language used.
   * @param {string} id The ID of the string to translate.
   * @param {SanitizeInnerHtmlOpts=} opts
   * @return {string} A translated, sanitized, substituted string.
   */
  i18nAdvancedDynamic(locale, id, opts) {
    return I18nBehavior.i18nAdvanced(id, opts);
  },

  i18nUpdateLocale() {
    // TODO(crbug.com/955194): move i18nUpdateLocale from I18nBehavior to this
    // class.
    I18nBehavior.i18nUpdateLocale.call(this);
    var matches = this.shadowRoot.querySelectorAll('.i18n-dynamic');
    for (var child of matches) {
      if (typeof (child.i18nUpdateLocale) === 'function') {
        child.i18nUpdateLocale();
      }
    }
  },
};

/**
 * TODO: Replace with an interface. b/24294625
 * @typedef {{
 *   i18nUpdateLocale: function()
 * }}
 */
OobeI18nBehaviorImpl.Proto;
/** @polymerBehavior */
export const OobeI18nBehavior = [I18nBehavior, OobeI18nBehaviorImpl];

/** @interface */
export class OobeI18nBehaviorInterface extends I18nBehaviorInterface {
  /**
   * @param {string} id The ID of the string to translate.
   * @param {...string|number} var_args
   * @return {string}
   */
  i18n(id, var_args) {}
  i18nUpdateLocale() {}

  /**
   * @param {string} locale The UI language used.
   * @param {string} id The ID of the string to translate.
   * @param {SanitizeInnerHtmlOpts=} opts
   * @return {string} A translated, sanitized, substituted string.
   */
  i18nAdvancedDynamic(locale, id, opts) {}
}
