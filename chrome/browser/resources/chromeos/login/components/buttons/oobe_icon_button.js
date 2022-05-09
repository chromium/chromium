// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
    is: 'oobe-icon-button',

    behaviors: [OobeI18nBehavior],

    properties: {
      icon1x: {type: String, observer: 'updateIconVisibility_'},
      icon2x: String,


      /* The ID of the localized string to be used as button text.
       */
      textKey: {
        type: String,
      },

      labelForAria: {
        type: String,
      },

      labelForAriaText_: {
        type: String,
        computed: 'ariaLabel_(labelForAria, locale, textKey)',
      },
    },

    focus() {
      this.$.button.focus();
    },

    updateIconVisibility_() {
      this.$.icon.hidden = (this.icon1x === undefined || this.icon1x.length == 0);
    },

    ariaLabel_(labelForAria, locale, textKey) {
      if ((typeof labelForAria !== 'undefined') && (labelForAria !== '')) {
        return labelForAria;
      }
      return this.i18n(textKey);
    },
  });
