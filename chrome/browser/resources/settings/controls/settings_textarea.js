// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-textarea' is a component similar to native textarea,
 * and inherits styling from cr-input.
 */
Polymer({
  is: 'settings-textarea',

  properties: {
    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    label: {
      type: String,
      value: '',
    },

    value: {
      type: String,
      value: '',
      notify: true,
    },
  },

  hostAttributes: {
    'aria-disabled': 'false',
  },

  /**
   * 'change' event fires when <input> value changes and user presses 'Enter'.
   * This function helps propagate it to host since change events don't
   * propagate across Shadow DOM boundary by default.
   * @param {!Event} e
   * @private
   */
  onInputChange_: function(e) {
    this.fire('change', {sourceEvent: e});
  },

  // focused_ is used instead of :focus-within, so focus on elements within the
  // suffix slot does not trigger a change in input styles.
  onInputFocusChange_: function() {
    if (this.shadowRoot.activeElement == this.$.input)
      this.setAttribute('focused_', '');
    else
      this.removeAttribute('focused_');
  },
});
