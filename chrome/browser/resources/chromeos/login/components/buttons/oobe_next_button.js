// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-next-button',

  behaviors: [OobeI18nBehavior],

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    /* The ID of the localized string to be used as button text.
     */
    textKey: {
      type: String,
      value: 'next',
    },

    labelForAria: {
      type: String,
    },

    labelForAria_: {
      type: String,
      computed: 'ariaLabel_(labelForAria, locale, textKey)',
    },
  },

  focus() {
    this.$.button.focus();
  },

  onClick_(e) {
    // Just checking here. The event is propagated further.
    assert(!this.disabled);
  },

  ariaLabel_(labelForAria, locale, textKey) {
    if ((typeof labelForAria !== 'undefined') && (labelForAria !== '')) {
      return labelForAria;
    }
    return this.i18n(textKey);
  },
});
