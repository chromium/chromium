// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'gaia-button',

  properties: {
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    link: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'onLinkChanged_',
    },
  },

  focus: function() {
    this.$.button.focus();
  },

  /** @private */
  onLinkChanged_: function() {
    this.$.button.classList.toggle('action-button', !this.link);
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
  is: 'gaia-icon-button',

  properties: {
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    icon: String,

    ariaLabel: String,
  },

  focus: function() {
    this.$.iconButton.focus();
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
