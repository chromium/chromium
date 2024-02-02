// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import './print_management_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getMetadataProvider} from './mojo_interface_provider.js';
import {getTemplate} from './print_job_clear_history_dialog.html.js';

/**
 * @fileoverview
 * 'print-job-clear-history-dialog' is a confirmation dialog for clearing the
 * print job history
 */

const PrintJobClearHistoryDialogElementBase = I18nMixin(PolymerElement);

interface PrintJobClearHistoryDialogElement {
  $: {clearDialog: CrDialogElement};
}

class PrintJobClearHistoryDialogElement extends
    PrintJobClearHistoryDialogElementBase {
  static get is(): string {
    return 'print-job-clear-history-dialog';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      shouldDisableClearButton: {
        type: Boolean,
        value: false,
      },
    };
  }

  private shouldDisableClearButton: boolean;
  private mojoInterfaceProvider = getMetadataProvider();

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.clearDialog.showModal();
  }

  private onCancelButtonClick(): void {
    this.$.clearDialog.close();
  }

  private onClearButtonClick(): void {
    this.shouldDisableClearButton = true;
    this.mojoInterfaceProvider.deleteAllPrintJobs().then(
        this.onClearedHistory.bind(this));
  }

  private onClearedHistory(clearedHistoryResult: {success: boolean}): void {
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