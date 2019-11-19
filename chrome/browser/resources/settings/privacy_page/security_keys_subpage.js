// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'security-keys-subpage' is a settings subpage
 * containing operations on security keys.
 */
Polymer({
  is: 'security-keys-subpage',

  properties: {
    /** @private */
    enableBioEnrollment_: {
      type: Boolean,
      readOnly: true,
      value: function() {
        return loadTimeData.getBoolean('enableSecurityKeysBioEnrollment');
      }
    },

    /** @private */
    showSetPINDialog_: {
      type: Boolean,
      value: false,
    },
    /** @private */
    showCredentialManagementDialog_: {
      type: Boolean,
      value: false,
    },
    /** @private */
    showResetDialog_: {
      type: Boolean,
      value: false,
    },
    /** @private */
    showBioEnrollDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onSetPIN_: function() {
    this.showSetPINDialog_ = true;
  },

  /** @private */
  onSetPINDialogClosed_: function() {
    this.showSetPINDialog_ = false;
    cr.ui.focusWithoutInk(this.$.setPINButton);
  },

  /** @private */
  onCredentialManagement_: function() {
    this.showCredentialManagementDialog_ = true;
  },

  /** @private */
  onCredentialManagementDialogClosed_: function() {
    this.showCredentialManagementDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#credentialManagementButton')));
  },

  /** @private */
  onReset_: function() {
    this.showResetDialog_ = true;
  },

  /** @private */
  onResetDialogClosed_: function() {
    this.showResetDialog_ = false;
    cr.ui.focusWithoutInk(this.$.resetButton);
  },

  /** @private */
  onBioEnroll_: function() {
    this.showBioEnrollDialog_ = true;
  },

  /** @private */
  onBioEnrollDialogClosed_: function() {
    this.showBioEnrollDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#bioEnrollButton')));
  },
});
