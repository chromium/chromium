// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-export-dialog' is the dialog that allows exporting
 * passwords.
 */

import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy, PasswordsFileExportProgressListener} from '../password_manager_proxy.js';

import {getTemplate} from './passwords_export_dialog.html.js';

/**
 * The states of the export passwords dialog.
 */
enum States {
  IN_PROGRESS = 'IN_PROGRESS',
  ERROR = 'ERROR',
}

const ProgressStatus = chrome.passwordsPrivate.ExportProgressStatus;

/**
 * The amount of time (ms) between the start of the export and the moment we
 * start showing the progress bar.
 */
const progressBarDelayMs: number = 100;

/**
 * The minimum amount of time (ms) that the progress bar will be visible.
 */
const progressBarBlockMs: number = 1000;


const PasswordsExportDialogElementBase = I18nMixin(PolymerElement);

export class PasswordsExportDialogElement extends
    PasswordsExportDialogElementBase {
  static get is() {
    return 'passwords-export-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Do not change these variables directly to show dialogs, call
       * switchToDialog_ which guarantees two dialogs are not shown at the same
       * time.
       */
      showProgressDialog_: Boolean,
      showErrorDialog_: Boolean,

      /** The error that occurred while exporting. */
      exportErrorMessage: String,
    };
  }

  exportErrorMessage: string;
  private showProgressDialog_: boolean;
  private showErrorDialog_: boolean;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();
  private onPasswordsFileExportProgressListener_:
      PasswordsFileExportProgressListener|null = null;
  // Token for the timeout used to ensure that the progress bar is not shown if
  // the export takes less than |progressBarDelayMs|.
  private progressBarDelayToken_: number|null;
  // Token for the timeout used to ensure that the progress bar is visible for
  // at least |progressBarBlockMs|.
  private progressBarBlockToken_: number|null;
  private delayedProgress_: chrome.passwordsPrivate.PasswordExportProgress|null;

  override ready() {
    super.ready();
    this.addEventListener('cancel', this.close_);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.onPasswordsFileExportProgressListener_ =
        (progress: chrome.passwordsPrivate.PasswordExportProgress) =>
            this.onPasswordsFileExportProgress_(progress);

    // If export started on a different tab and is still in progress, display a
    // busy UI.
    this.passwordManager_.requestExportProgressStatus().then(status => {
      if (status === ProgressStatus.IN_PROGRESS) {
        this.switchToDialog_(States.IN_PROGRESS);
      }
    });

    this.passwordManager_.addPasswordsFileExportProgressListener(
        this.onPasswordsFileExportProgressListener_);

    this.startExport_();
  }

  /**
   * Handles an export progress event by changing the visible dialog or caching
   * the event for later consumption.
   */
  private onPasswordsFileExportProgress_(
      progress: chrome.passwordsPrivate.PasswordExportProgress) {
    // If Chrome has already started displaying the progress bar
    // (|progressBarDelayToken_ is null) and hasn't completed its minimum
    // display time (|progressBarBlockToken_| is not null) progress should be
    // cached for consumption when the blocking time ends.
    const progressBlocked =
        !this.progressBarDelayToken_ && this.progressBarBlockToken_;
    if (!progressBlocked) {
      clearTimeout(this.progressBarDelayToken_!);
      this.progressBarDelayToken_ = null;
      this.processProgress_(progress);
    } else {
      this.delayedProgress_ = progress;
    }
  }

  /**
   * Displays the progress bar and suspends further UI updates for
   * |progressBarBlockMs|.
   */
  private handleProgressBarDisplay_() {
    this.progressBarDelayToken_ = null;
    this.switchToDialog_(States.IN_PROGRESS);
    this.progressBarBlockToken_ =
        setTimeout(() => this.processDelayedProgress_(), progressBarBlockMs);
  }

  /**
   * Unblocks progress after showing the progress bar for |progressBarBlock|ms
   * and processes any progress that was delayed.
   */
  private processDelayedProgress_() {
    this.progressBarBlockToken_ = null;
    if (this.delayedProgress_) {
      this.processProgress_(this.delayedProgress_);
      this.delayedProgress_ = null;
    }
  }

  /** Closes the dialog. */
  private close_() {
    clearTimeout(this.progressBarDelayToken_!);
    clearTimeout(this.progressBarBlockToken_!);
    this.progressBarDelayToken_ = null;
    this.progressBarBlockToken_ = null;
    this.passwordManager_.removePasswordsFileExportProgressListener(
        this.onPasswordsFileExportProgressListener_!);
    this.showProgressDialog_ = false;
    this.showErrorDialog_ = false;
    assert(this.onPasswordsFileExportProgressListener_);
    this.passwordManager_.removePasswordsFileExportProgressListener(
        this.onPasswordsFileExportProgressListener_);
    // Need to allow for the dialogs to be removed from the DOM before firing
    // the close event. Otherwise the handler will not be able to set focus.
    microTask.run(
        () => this.dispatchEvent(new CustomEvent(
            'passwords-export-dialog-close', {bubbles: true, composed: true})));
  }

  /**
   * Tells the PasswordsPrivate API to export saved passwords in a .csv.
   */
  private startExport_() {
    this.passwordManager_.exportPasswords().catch((error) => {
      if (error === 'in-progress') {
        // Exporting was started by a different call to exportPasswords() and is
        // is still in progress. This UI needs to be updated to the current
        // status.
        this.switchToDialog_(States.IN_PROGRESS);
      }
    });
  }

  /**
   * Prepares and displays the appropriate view (with delay, if necessary).
   */
  private processProgress_(progress:
                               chrome.passwordsPrivate.PasswordExportProgress) {
    if (progress.status === ProgressStatus.IN_PROGRESS) {
      this.progressBarDelayToken_ = setTimeout(
          () => this.handleProgressBarDisplay_(), progressBarDelayMs);
      return;
    }
    if (progress.status === ProgressStatus.SUCCEEDED) {
      // TODO(crbug/1394416): Maybe notify the user of successful completion.
      this.close_();
      return;
    }
    if (progress.status === ProgressStatus.FAILED_WRITE_FAILED) {
      this.exportErrorMessage =
          this.i18n('exportPasswordsFailTitle', progress.folderName!);
      this.switchToDialog_(States.ERROR);
      return;
    }
  }

  /**
   * Opens the specified dialog and hides the others.
   * @param state the dialog to open.
   */
  private switchToDialog_(state: States) {
    this.showProgressDialog_ = state === States.IN_PROGRESS;
    this.showErrorDialog_ = state === States.ERROR;
  }

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   */
  private onCancelButtonTap_() {
    this.close_();
  }

  /**
   * Handler for tapping the 'cancel' button on the progress dialog. It should
   * cancel the export and dismiss the dialog.
   */
  private onCancelProgressButtonTap_() {
    this.passwordManager_.cancelExportPasswords();
    this.close_();
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'passwords-export-dialog': PasswordsExportDialogElement;
  }
}

customElements.define(
    PasswordsExportDialogElement.is, PasswordsExportDialogElement);