// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-text-button',

  behaviors: [OobeI18nBehavior],

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    inverse: {
      type: Boolean,
      observer: 'onInverseChanged_',
    },

    /* The ID of the localized string to be used as button text.
     */
    textKey: {
      type: String,
    },

    border: Boolean,

    labelForAria: {
      type: String,
    },

    labelForAriaText_: {
      type: String,
      computed: 'ariaLabel_(labelForAria, locale, textKey)',
    },
  },

  focus() {
    this.$.textButton.focus();
  },

  onClick_(e) {
    if (this.disabled)
      e.stopPropagation();
  },

  onInverseChanged_() {
    this.$.textButton.classList.toggle('action-button', this.inverse);
  },

  ariaLabel_(labelForAria, locale, textKey) {
    if ((typeof labelForAria !== 'undefined') && (labelForAria !== '')) {
      return labelForAria;
    }
    return this.i18n(textKey);
  },
});

Polymer({
  is: 'oobe-back-button',

  behaviors: [OobeI18nBehavior],

  properties: {
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /* The ID of the localized string to be used as button text.
     */
    textKey: {
      type: String,
      value: 'back',
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

  /**
   * @param {!Event} e
   * @private
   */
  onClick_(e) {
    if (this.disabled) {
      e.stopPropagation();
    }
  },

  ariaLabel_(labelForAria, locale, textKey) {
    if ((typeof labelForAria !== 'undefined') && (labelForAria !== '')) {
      return labelForAria;
    }
    return this.i18n(textKey);
  },
});

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
    if (this.disabled)
      e.stopPropagation();
  },

  ariaLabel_(labelForAria, locale, textKey) {
    if ((typeof labelForAria !== 'undefined') && (labelForAria !== '')) {
      return labelForAria;
    }
    return this.i18n(textKey);
  },
});

Polymer({
  is: 'oobe-welcome-secondary-button',

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

  click() {
    this.$.button.click();
  },

  ariaLabel_(labelForAria, locale, textKey) {
    if ((typeof labelForAria !== 'undefined') && (labelForAria !== '')) {
      return labelForAria;
    }
    return this.i18n(textKey);
  },
});
