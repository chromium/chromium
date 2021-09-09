// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './print_management_shared_css.js';
import './printing_manager.mojom-lite.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getMetadataProvider} from './mojo_interface_provider.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';

/**
 * @fileoverview
 * 'print-job-clear-history-dialog' is a confirmation dialog for clearing the
 * print job history
 */
Polymer({
  is: 'print-job-clear-history-dialog',

  _template: html`{__html_template__}`,

  properties: {
    /** @private */
    shouldDisableClearButton_: {
      type: Boolean,
      value: false
    },
  },

  behaviors: [I18nBehavior],

  /** @override */
  created() {
    this.mojoInterfaceProvider_ = getMetadataProvider();
  },

  /** @override */
  attached() {
    this.$.clearDialog.showModal();
  },

  /** @private */
  onCancelButtonClick_() {
    this.$.clearDialog.close();
  },

  /** @private */
  onClearButtonClick_() {
    this.shouldDisableClearButton_ = true;
    this.mojoInterfaceProvider_.deleteAllPrintJobs()
      .then(this.onClearedHistory_.bind(this))
  },

  /**
   * @param {!{success: boolean}} clearedHistoryResult
   * @private
   */
  onClearedHistory_(clearedHistoryResult) {
    if (clearedHistoryResult.success) {
      this.fire('all-history-cleared');
    }
    // Failed case results in a no op and closes the dialog.
    // |clearedHistoryResult| is temporarily set to false until the policy
    // to control print job history deletions is implemented.
    this.$.clearDialog.close();
  },
});