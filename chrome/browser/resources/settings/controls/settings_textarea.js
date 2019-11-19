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
    /**
     * Whether the text area should automatically get focus when the page
     * loads.
     */
    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Whether the text area is disabled. When disabled, the text area loses
     * focus and is not reachable by tabbing.
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: 'onDisabledChanged_'
    },

    /** Number of rows (lines) of the text area. */
    rows: {
      type: Number,
      value: 3,
      reflectToAttribute: true,
    },

    /** Caption of the text area. */
    label: {
      type: String,
      value: '',
    },

    /**
     * Text inside the text area. If the text exceeds the bounds of the text
     * area, i.e. if it has more than |rows| lines, a scrollbar is shown by
     * default.
     */
    value: {
      type: String,
      value: '',
      notify: true,
    },
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

  /**@private */
  onInputFocusChange_: function() {
    // focused_ is used instead of :focus-within, so focus on elements within
    // the suffix slot does not trigger a change in input styles.
    if (this.shadowRoot.activeElement == this.$.input) {
      this.setAttribute('focused_', '');
    } else {
      this.removeAttribute('focused_');
    }
  },

  /**@private */
  onDisabledChanged_: function() {
    this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
  },
});
