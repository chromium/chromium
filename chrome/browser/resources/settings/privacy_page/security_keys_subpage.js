// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'security-keys-subpage' is a settings subpage
 * containing operations on security keys.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_shared_css.js';
import './security_keys_credential_management_dialog.js';
import './security_keys_bio_enroll_dialog.js';
import './security_keys_set_pin_dialog.js';
import './security_keys_reset_dialog.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

Polymer({
  is: 'security-keys-subpage',

  _template: html`{__html_template__}`,

  properties: {
    /** @private */
    enableBioEnrollment_: {
      type: Boolean,
      readOnly: true,
      value() {
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
  onSetPIN_() {
    this.showSetPINDialog_ = true;
  },

  /** @private */
  onSetPINDialogClosed_() {
    this.showSetPINDialog_ = false;
    focusWithoutInk(this.$.setPINButton);
  },

  /** @private */
  onCredentialManagement_() {
    this.showCredentialManagementDialog_ = true;
  },

  /** @private */
  onCredentialManagementDialogClosed_() {
    this.showCredentialManagementDialog_ = false;
    focusWithoutInk(assert(this.$$('#credentialManagementButton')));
  },

  /** @private */
  onReset_() {
    this.showResetDialog_ = true;
  },

  /** @private */
  onResetDialogClosed_() {
    this.showResetDialog_ = false;
    focusWithoutInk(this.$.resetButton);
  },

  /** @private */
  onBioEnroll_() {
    this.showBioEnrollDialog_ = true;
  },

  /** @private */
  onBioEnrollDialogClosed_() {
    this.showBioEnrollDialog_ = false;
    focusWithoutInk(assert(this.$$('#bioEnrollButton')));
  },
});
