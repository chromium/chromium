// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-export-dialog' is the dialog that allows exporting
 * passwords.
 */

(function() {
'use strict';

/**
 * The states of the export passwords dialog.
 * @enum {string}
 */
const States = {
  START: 'START',
  IN_PROGRESS: 'IN_PROGRESS',
  ERROR: 'ERROR',
};

const ProgressStatus = chrome.passwordsPrivate.ExportProgressStatus;

/**
 * The amount of time (ms) between the start of the export and the moment we
 * start showing the progress bar.
 * @type {number}
 */
const progressBarDelayMs = 100;

/**
 * The minimum amount of time (ms) that the progress bar will be visible.
 * @type {number}
 */
const progressBarBlockMs = 1000;

Polymer({
  is: 'passwords-export-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /** The error that occurred while exporting. */
    exportErrorMessage: String,

    /** @private */
    showStartDialog_: Boolean,

    /** @private */
    showProgressDialog_: Boolean,

    /** @private */
    showErrorDialog_: Boolean,

    // <if expr="chromeos">
    /** @type settings.BlockingRequestManager */
    tokenRequestManager: Object
    // </if>
  },

  listeners: {
    'cancel': 'close',
  },

  /**
   * The interface for callbacks to the browser.
   * Defined in passwords_section.js
   * @type {PasswordManagerProxy}
   * @private
   */
  passwordManager_: null,

  /** @private {function(!PasswordManagerProxy.PasswordExportProgress):void} */
  onPasswordsFileExportProgressListener_: null,

  /**
   * The task that will display the progress bar, if the export doesn't finish
   * quickly. This is null, unless the task is currently scheduled.
   * @private {?number}
   */
  progressTaskToken_: null,

  /**
   * The task that will display the completion of the export, if any. We display
   * the progress bar for at least |progressBarBlockMs|, therefore, if export
   * finishes earlier, we cache the result in |delayedProgress_| and this task
   * will consume it. This is null, unless the task is currently scheduled.
   * @private {?number}
   */
  delayedCompletionToken_: null,

  /**
   * We display the progress bar for at least |progressBarBlockMs|. If progress
   * is achieved earlier, we store the update here and consume it later.
   * @private {?PasswordManagerProxy.PasswordExportProgress}
   */
  delayedProgress_: null,

  /** @override */
  attached: function() {
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    this.switchToDialog_(States.START);

    this.onPasswordsFileExportProgressListener_ =
        this.onPasswordsFileExportProgress_.bind(this);

    // If export started on a different tab and is still in progress, display a
    // busy UI.
    this.passwordManager_.requestExportProgressStatus(status => {
      if (status == ProgressStatus.IN_PROGRESS) {
        this.switchToDialog_(States.IN_PROGRESS);
      }
    });

    this.passwordManager_.addPasswordsFileExportProgressListener(
        this.onPasswordsFileExportProgressListener_);
  },

  /**
   * Handles an export progress event by changing the visible dialog or caching
   * the event for later consumption.
   * @param {!PasswordManagerProxy.PasswordExportProgress} progress
   * @private
   */
  onPasswordsFileExportProgress_(progress) {
    // If Chrome has already started displaying the progress bar
    // (|progressTaskToken_ is null) and hasn't completed its minimum display
    // time (|delayedCompletionToken_| is not null) progress should be cached
    // for consumption when the blocking time ends.
    const progressBlocked =
        !this.progressTaskToken_ && this.delayedCompletionToken_;
    if (!progressBlocked) {
      clearTimeout(this.progressTaskToken_);
      this.progressTaskToken_ = null;
      this.processProgress_(progress);
    } else {
      this.delayedProgress_ = progress;
    }
  },

  /**
   * Displays the progress bar and suspends further UI updates for
   * |progressBarBlockMs|.
   * @private
   */
  progressTask_() {
    this.progressTaskToken_ = null;
    this.switchToDialog_(States.IN_PROGRESS);

    this.delayedCompletionToken_ =
        setTimeout(this.delayedCompletionTask_.bind(this), progressBarBlockMs);
  },

  /**
   * Unblocks progress after showing the progress bar for |progressBarBlock|ms
   * and processes any progress that was delayed.
   * @private
   */
  delayedCompletionTask_() {
    this.delayedCompletionToken_ = null;
    if (this.delayedProgress_) {
      this.processProgress_(this.delayedProgress_);
      this.delayedProgress_ = null;
    }
  },

  /** Closes the dialog. */
  close: function() {
    clearTimeout(this.progressTaskToken_);
    clearTimeout(this.delayedCompletionToken_);
    this.progressTaskToken_ = null;
    this.delayedCompletionToken_ = null;
    this.passwordManager_.removePasswordsFileExportProgressListener(
        this.onPasswordsFileExportProgressListener_);
    this.showStartDialog_ = false;
    this.showProgressDialog_ = false;
    this.showErrorDialog_ = false;
    // Need to allow for the dialogs to be removed from the DOM before firing
    // the close event. Otherwise the handler will not be able to set focus.
    this.async(() => this.fire('passwords-export-dialog-close'));
  },

  /** @private */
  onExportTap_: function() {
    // <if expr="chromeos">
    this.tokenRequestManager.request(this.exportPasswords_.bind(this));
    // </if>
    // <if expr="not chromeos">
    this.exportPasswords_();
    // </if>
  },

  /**
   * Tells the PasswordsPrivate API to export saved passwords in a .csv pending
   * security checks.
   * @private
   */
  exportPasswords_: function() {
    this.passwordManager_.exportPasswords(() => {
      if (chrome.runtime.lastError &&
          chrome.runtime.lastError.message == 'in-progress') {
        // Exporting was started by a different call to exportPasswords() and is
        // is still in progress. This UI needs to be updated to the current
        // status.
        this.switchToDialog_(States.IN_PROGRESS);
      }
    });
  },

  /**
   * Prepares and displays the appropriate view (with delay, if necessary).
   * @param {!PasswordManagerProxy.PasswordExportProgress} progress
   * @private
   */
  processProgress_(progress) {
    if (progress.status == ProgressStatus.IN_PROGRESS) {
      this.progressTaskToken_ =
          setTimeout(this.progressTask_.bind(this), progressBarDelayMs);
      return;
    }
    if (progress.status == ProgressStatus.SUCCEEDED) {
      this.close();
      return;
    }
    if (progress.status == ProgressStatus.FAILED_WRITE_FAILED) {
      this.exportErrorMessage =
          this.i18n('exportPasswordsFailTitle', progress.folderName);
      this.switchToDialog_(States.ERROR);
      return;
    }
  },

  /**
   * Opens the specified dialog and hides the others.
   * @param {!States} state the dialog to open.
   * @private
   */
  switchToDialog_(state) {
    this.showStartDialog_ = state == States.START;
    this.showProgressDialog_ = state == States.IN_PROGRESS;
    this.showErrorDialog_ = state == States.ERROR;
  },

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancelButtonTap_: function() {
    this.close();
  },

  /**
   * Handler for tapping the 'cancel' button on the progress dialog. It should
   * cancel the export and dismiss the dialog.
   * @private
   */
  onCancelProgressButtonTap_: function() {
    this.passwordManager_.cancelExportPasswords();
    this.close();
  },
});
})();
