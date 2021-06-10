// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
   * See documentation for I18nBehavior.i18n(...)
   * @param {string} id The ID of the string to translate.
   * @param {...string|number} var_args Values to replace the placeholders $1
   *     to $9 in the string.
   * @return {string} A translated, sanitized, substituted string.
   */
  i18n(id, var_args) {
    if (typeof this.locale === 'undefined')
      return '';
    if (typeof id === 'undefined')
      return '';
    return I18nBehavior.i18n.apply(this, arguments);
  },

  i18nUpdateLocale() {
    // TODO(crbug.com/955194): move i18nUpdateLocale from I18nBehavior to this
    // class.
    I18nBehavior.i18nUpdateLocale.call(this);
    var matches = Polymer.dom(this.root).querySelectorAll('.i18n-dynamic');
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
/* #export */ const OobeI18nBehavior = [I18nBehavior, OobeI18nBehaviorImpl];
