// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-export-dialog' is the dialog that allows exporting
 * passwords.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
// <if expr="is_chromeos">
import '../controls/password_prompt_dialog.js';
// </if>
import '../settings_shared.css.js';
import './passwords_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="is_chromeos">
import {loadTimeData} from '../i18n_setup.js';
// </if>

import {PasswordManagerImpl, PasswordManagerProxy, PasswordsFileExportProgressListener} from './password_manager_proxy.js';
import {PasswordRequestorMixin} from './password_requestor_mixin.js';
import {getTemplate} from './passwords_export_dialog.html.js';


/**
 * The states of the export passwords dialog.
 */
enum States {
  START = 'START',
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


const PasswordsExportDialogElementBase =
    PasswordRequestorMixin(I18nMixin(PolymerElement));

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
      /** The error that occurred while exporting. */
      exportErrorMessage: String,

      showStartDialog_: Boolean,
      showProgressDialog_: Boolean,
      showErrorDialog_: Boolean,
    };
  }

  exportErrorMessage: string;
  private showStartDialog_: boolean;
  private showProgressDialog_: boolean;
  private showErrorDialog_: boolean;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();
  private onPasswordsFileExportProgressListener_:
      PasswordsFileExportProgressListener|null = null;
  private progressTaskToken_: number|null;
  private delayedCompletionToken_: number|null;
  private delayedProgress_: chrome.passwordsPrivate.PasswordExportProgress|null;

  constructor() {
    super();

    /**
     * The task that will display the progress bar; if the export doesn't finish
     * quickly. This is null; unless the task is currently scheduled.
     */
    this.progressTaskToken_ = null;

    /**
     * The task that will display the completion of the export; if any. We
     * display the progress bar for at least |progressBarBlockMs|; therefore, if
     * export finishes earlier; we cache the result in |delayedProgress_| and
     * this task will consume it. This is null; unless the task is currently
     * scheduled.
     */
    this.delayedCompletionToken_ = null;

    /**
     * We display the progress bar for at least |progressBarBlockMs|. If
     * progress is achieved earlier; we store the update here and consume it
     * later.
     */
    this.delayedProgress_ = null;
  }

  override ready() {
    super.ready();
    this.addEventListener('cancel', this.close);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.switchToDialog_(States.START);

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
  }

  /**
   * Handles an export progress event by changing the visible dialog or caching
   * the event for later consumption.
   */
  private onPasswordsFileExportProgress_(
      progress: chrome.passwordsPrivate.PasswordExportProgress) {
    // If Chrome has already started displaying the progress bar
    // (|progressTaskToken_ is null) and hasn't completed its minimum display
    // time (|delayedCompletionToken_| is not null) progress should be cached
    // for consumption when the blocking time ends.
    const progressBlocked =
        !this.progressTaskToken_ && this.delayedCompletionToken_;
    if (!progressBlocked) {
      clearTimeout(this.progressTaskToken_!);
      this.progressTaskToken_ = null;
      this.processProgress_(progress);
    } else {
      this.delayedProgress_ = progress;
    }
  }

  /**
   * Displays the progress bar and suspends further UI updates for
   * |progressBarBlockMs|.
   */
  private progressTask_() {
    this.progressTaskToken_ = null;
    this.switchToDialog_(States.IN_PROGRESS);

    this.delayedCompletionToken_ =
        setTimeout(() => this.delayedCompletionTask_(), progressBarBlockMs);
  }

  /**
   * Unblocks progress after showing the progress bar for |progressBarBlock|ms
   * and processes any progress that was delayed.
   */
  private delayedCompletionTask_() {
    this.delayedCompletionToken_ = null;
    if (this.delayedProgress_) {
      this.processProgress_(this.delayedProgress_);
      this.delayedProgress_ = null;
    }
  }

  /** Closes the dialog. */
  close() {
    clearTimeout(this.progressTaskToken_!);
    clearTimeout(this.delayedCompletionToken_!);
    this.progressTaskToken_ = null;
    this.delayedCompletionToken_ = null;
    this.passwordManager_.removePasswordsFileExportProgressListener(
        this.onPasswordsFileExportProgressListener_!);
    this.showStartDialog_ = false;
    this.showProgressDialog_ = false;
    this.showErrorDialog_ = false;
    // Need to allow for the dialogs to be removed from the DOM before firing
    // the close event. Otherwise the handler will not be able to set focus.
    microTask.run(
        () => this.dispatchEvent(new CustomEvent(
            'passwords-export-dialog-close', {bubbles: true, composed: true})));
  }

  private onExportTap_() {
    // <if expr="is_chromeos">
    if (loadTimeData.getBoolean('useSystemAuthenticationForPasswordManager')) {
      this.exportPasswords_();
      return;
    }
    this.tokenRequestManager.request(() => this.exportPasswords_());
    // </if>
    // <if expr="not is_chromeos">
    this.exportPasswords_();
    // </if>
  }

  /**
   * Tells the PasswordsPrivate API to export saved passwords in a .csv pending
   * security checks.
   */
  private exportPasswords_() {
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
      this.progressTaskToken_ =
          setTimeout(() => this.progressTask_(), progressBarDelayMs);
      return;
    }
    if (progress.status === ProgressStatus.SUCCEEDED) {
      this.close();
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
    this.showStartDialog_ = state === States.START;
    this.showProgressDialog_ = state === States.IN_PROGRESS;
    this.showErrorDialog_ = state === States.ERROR;
  }

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   */
  private onCancelButtonTap_() {
    this.close();
  }

  /**
   * Handler for tapping the 'cancel' button on the progress dialog. It should
   * cancel the export and dismiss the dialog.
   */
  private onCancelProgressButtonTap_() {
    this.passwordManager_.cancelExportPasswords();
    this.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-export-dialog': PasswordsExportDialogElement;
  }
}

customElements.define(
    PasswordsExportDialogElement.is, PasswordsExportDialogElement);
