// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-text-button',

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    inverse: {
      type: Boolean,
      observer: 'onInverseChanged_',
    },

    border: Boolean,

    /* Note that we are not using "aria-label" property here, because
     * we want to pass the label value but not actually declare it as an
     * ARIA property anywhere but the actual target element.
     */
    labelForAria: String,
  },

  focus: function() {
    this.$.textButton.focus();
  },

  onClick_: function(e) {
    if (this.disabled)
      e.stopPropagation();
  },

  onInverseChanged_: function() {
    this.$.textButton.classList.toggle('action-button', this.inverse);
  },
});

Polymer({
  is: 'oobe-back-button',

  properties: {
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /* Note that we are not using "aria-label" property here, because
     * we want to pass the label value but not actually declare it as an
     * ARIA property anywhere but the actual target element.
     */
    labelForAria: String,
  },

  focus: function() {
    this.$.button.focus();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onClick_: function(e) {
    if (this.disabled) {
      e.stopPropagation();
    }
  },
});

Polymer({
  is: 'oobe-next-button',

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},
  },

  focus: function() {
    this.$.button.focus();
  },

  onClick_: function(e) {
    if (this.disabled)
      e.stopPropagation();
  }
});

Polymer({
  is: 'oobe-welcome-secondary-button',

  properties: {
    icon1x: {type: String, observer: 'updateIconVisibility_'},
    icon2x: String,

    /* Note that we are not using "aria-label" property here, because
     * we want to pass the label value but not actually declare it as an
     * ARIA property anywhere but the actual target element.
     */
    labelForAria: String
  },

  focus: function() {
    this.$.button.focus();
  },

  updateIconVisibility_: function() {
    this.$.icon.hidden = (this.icon1x === undefined || this.icon1x.length == 0);
  },

  click: function() {
    this.$.button.click();
  },
});
