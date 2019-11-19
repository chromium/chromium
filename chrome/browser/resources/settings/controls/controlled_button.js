// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'controlled-button',

  behaviors: [
    CrPolicyPrefBehavior,
    PrefControlBehavior,
  ],

  properties: {
    endJustified: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    label: String,

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private */
    actionClass_: {
      type: String,
      value: ''
    },

    /** @private */
    enforced_: {
      type: Boolean,
      computed: 'isPrefEnforced(pref.*)',
      reflectToAttribute: true,
    },
  },

  /** @override */
  attached: function() {
    if (this.classList.contains('action-button')) {
      this.actionClass_ = 'action-button';
    }
  },

  /** Focus on the inner cr-button. */
  focus: function() {
    this.$$('cr-button').focus();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIndicatorTap_: function(e) {
    // Disallow <controlled-button on-click="..."> when controlled.
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @param {!boolean} enforced
   * @param {!boolean} disabled
   * @return {boolean} True if the button should be enabled.
   * @private
   */
  buttonEnabled_(enforced, disabled) {
    return !enforced && !disabled;
  }
});
