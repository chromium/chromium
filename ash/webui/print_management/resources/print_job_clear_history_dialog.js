// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import './print_management_shared.css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getMetadataProvider} from './mojo_interface_provider.js';
import {getTemplate} from './print_job_clear_history_dialog.html.js';

/**
 * @fileoverview
 * 'print-job-clear-history-dialog' is a confirmation dialog for clearing the
 * print job history
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const PrintJobClearHistoryDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class PrintJobClearHistoryDialogElement extends
    PrintJobClearHistoryDialogElementBase {
  static get is() {
    return 'print-job-clear-history-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @private */
      shouldDisableClearButton_: {
        type: Boolean,
        value: false,
      },

    };
  }

  /** @override */
  constructor() {
    super();

    this.mojoInterfaceProvider_ = getMetadataProvider();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.clearDialog.showModal();
  }

  /** @private */
  onCancelButtonClick_() {
    this.$.clearDialog.close();
  }

  /** @private */
  onClearButtonClick_() {
    this.shouldDisableClearButton_ = true;
    this.mojoInterfaceProvider_.deleteAllPrintJobs().then(
        this.onClearedHistory_.bind(this));
  }

  /**
   * @param {!{success: boolean}} clearedHistoryResult
   * @private
   */
  onClearedHistory_(clearedHistoryResult) {
    if (clearedHistoryResult.success) {
      this.dispatchEvent(new CustomEvent(
          'all-history-cleared', {bubbles: true, composed: true}));
    }
    // Failed case results in a no op and closes the dialog.
    // |clearedHistoryResult| is temporarily set to false until the policy
    // to control print job history deletions is implemented.
    this.$.clearDialog.close();
  }
}

customElements.define(
    PrintJobClearHistoryDialogElement.is, PrintJobClearHistoryDialogElement);