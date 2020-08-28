// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * Enum for the state of `crostini-upgrader-app`.
 * @enum {string}
 */
const State = {
  PROMPT: 'prompt',
  BACKUP: 'backup',
  BACKUP_SUCCEEDED: 'backupSucceeded',
  PRECHECKS_FAILED: 'prechecksFailed',
  UPGRADING: 'upgrading',
  UPGRADE_ERROR: 'upgrade_error',
  OFFER_RESTORE: 'offerRestore',
  RESTORE: 'restore',
  RESTORE_SUCCEEDED: 'restoreSucceeded',
  ERROR: 'error',
  CANCELING: 'canceling',
  SUCCEEDED: 'succeeded',
};

const kMaxUpgradeAttempts = 3;


Polymer({
  is: 'crostini-upgrader-app',

  _template: html`{__html_template__}`,

  properties: {
    /** @private {State} */
    state_: {
      type: String,
      value: State.PROMPT,
    },

    /** @private */
    backupCheckboxChecked_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    backupProgress_: {
      type: Number,
    },

    /** @private */
    upgradeProgress_: {
      type: Number,
      value: 0,
    },

    /** @private */
    restoreProgress_: {
      type: Number,
    },

    /** @private */
    progressMessages_: {
      type: Array,
      value: [],
    },

    /** @private */
    progressLineNumber_: {
      type: Number,
      value: 0,
    },

    /** @private */
    lastProgressLine_: {
      type: String,
      value: '',
    },

    /** @private */
    progressLineDisplayMs_: {
      type: Number,
      value: 300,
    },

    /** @private */
    upgradeAttemptCount_: {
      type: Number,
      value: 0,
    },

    /**
     * Enable the html template to use State.
     * @private
     */
    State: {
      type: Object,
      value: State,
    },
  },

  /** @override */
  attached() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;

    this.listenerIds_ = [
      callbackRouter.onBackupProgress.addListener((percent) => {
        this.state_ = State.BACKUP;
        this.backupProgress_ = percent;
      }),
      callbackRouter.onBackupSucceeded.addListener((wasCancelled) => {
        assert(this.state_ === State.BACKUP);
        this.state_ = State.BACKUP_SUCCEEDED;
        // We do a short (2 second) interstitial display of the backup success
        // message before continuing the upgrade.
        var timeout = new Promise((resolve, reject) => {
          setTimeout(resolve, wasCancelled ? 0 : 2000);
        });
        // We also want to wait for the prechecks to finish.
        var callback = new Promise((resolve, reject) => {
          this.startPrechecks_(resolve, reject);
        });
        Promise.all([timeout, callback]).then(() => {
          this.startUpgrade_();
        });
      }),
      callbackRouter.onBackupFailed.addListener(() => {
        assert(this.state_ === State.BACKUP);
        this.state_ = State.ERROR;
      }),
      callbackRouter.precheckStatus.addListener((status) => {
        this.precheckStatus_ = status;
        if (status ===
            chromeos.crostiniUpgrader.mojom.UpgradePrecheckStatus.OK) {
          this.precheckSuccessCallback_();
        } else {
          this.state_ = State.PRECHECKS_FAILED;
          this.precheckFailureCallback_();
        }
      }),
      callbackRouter.onUpgradeProgress.addListener((progressMessages) => {
        assert(this.state_ === State.UPGRADING);
        this.progressMessages_.push(...progressMessages);
        this.upgradeProgress_ = this.progressMessages_.length;

        if (this.progressLineNumber_ < this.upgradeProgress_) {
          this.updateProgressLine_();
        }
      }),
      callbackRouter.onUpgradeSucceeded.addListener(() => {
        assert(this.state_ === State.UPGRADING);
        this.state_ = State.SUCCEEDED;
      }),
      callbackRouter.onUpgradeFailed.addListener(() => {
        assert(this.state_ === State.UPGRADING);
        if (this.upgradeAttemptCount_ < kMaxUpgradeAttempts) {
          this.precheckThenUpgrade_();
          return;
        }
        if (this.backupCheckboxChecked_) {
          this.state_ = State.OFFER_RESTORE;
        } else {
          this.state_ = State.UPGRADE_ERROR;
        }
      }),
      callbackRouter.onRestoreProgress.addListener((percent) => {
        assert(this.state_ === State.RESTORE);
        this.restoreProgress_ = percent;
      }),
      callbackRouter.onRestoreSucceeded.addListener(() => {
        assert(this.state_ === State.RESTORE);
        this.state_ = State.RESTORE_SUCCEEDED;
      }),
      callbackRouter.onRestoreFailed.addListener(() => {
        assert(this.state_ === State.RESTORE);
        this.state_ = State.ERROR;
      }),
      callbackRouter.onCanceled.addListener(() => {
        if (this.state_ === State.RESTORE) {
          this.state_ = State.ERROR;
          return;
        }
        this.closePage_();
      }),
      callbackRouter.requestClose.addListener(() => {
        if (this.canCancel_(this.state_)) {
          this.onCancelButtonClick_();
        }
      })
    ];

    document.addEventListener('keyup', event => {
      if (event.key == 'Escape' && this.canCancel_(this.state_)) {
        this.onCancelButtonClick_();
        event.preventDefault();
      }
    });

    this.$$('.action-button').focus();
  },

  /** @override */
  detached() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => callbackRouter.removeListener(id));
  },

  /** @private */
  precheckThenUpgrade_() {
    this.startPrechecks_(() => {
      this.startUpgrade_();
    }, () => {});
  },

  /** @private */
  onActionButtonClick_() {
    switch (this.state_) {
      case State.SUCCEEDED:
      case State.RESTORE_SUCCEEDED:
        BrowserProxy.getInstance().handler.launch();
        this.closePage_();
        break;
      case State.PRECHECKS_FAILED:
        this.precheckThenUpgrade_();
        break;
      case State.PROMPT:
        if (this.backupCheckboxChecked_) {
          this.startBackup_(/*showFileChooser=*/ false);
        } else {
          this.precheckThenUpgrade_();
        }
        break;
      case State.OFFER_RESTORE:
        this.startRestore_();
        break;
    }
  },

  /** @private */
  onCancelButtonClick_() {
    switch (this.state_) {
      case State.PROMPT:
        BrowserProxy.getInstance().handler.cancelBeforeStart();
        break;
      case State.UPGRADING:
        this.state_ = State.CANCELING;
        BrowserProxy.getInstance().handler.cancel();
        break;
      case State.PRECHECKS_FAILED:
      case State.UPGRADE_ERROR:
      case State.ERROR:
      case State.OFFER_RESTORE:
      case State.SUCCEEDED:
        this.closePage_();
        break;
      case State.CANCELING:
        break;
      default:
        assertNotReached();
    }
  },

  /** @private */
  onChangeLocationButtonClick_() {
    this.startBackup_(/*showFileChooser=*/ true);
  },

  /**
   * @param {boolean} showFileChooser
   * @private
   */
  startBackup_(showFileChooser) {
    BrowserProxy.getInstance().handler.backup(showFileChooser);
  },

  /** @private */
  startPrechecks_(success, failure) {
    this.precheckSuccessCallback_ = success;
    this.precheckFailureCallback_ = failure;
    BrowserProxy.getInstance().handler.startPrechecks();
  },

  /** @private */
  startUpgrade_() {
    this.state_ = State.UPGRADING;
    this.upgradeAttemptCount_++;
    BrowserProxy.getInstance().handler.upgrade();
  },

  /** @private */
  startRestore_() {
    this.state_ = State.RESTORE;
    BrowserProxy.getInstance().handler.restore();
  },

  /** @private */
  closePage_() {
    BrowserProxy.getInstance().handler.onPageClosed();
  },

  /**
   * @param {State} state1
   * @param {State} state2
   * @return {boolean}
   * @private
   */
  isState_(state1, state2) {
    return state1 === state2;
  },

  /**
   * @param {State} state
   * @return {boolean}
   * @private
   */
  isProgressMessageHidden_(state) {
    return this.isState_(this.state_, State.PROMPT) ||
        this.isState_(this.state_, State.UPGRADE_ERROR) ||
        this.isState_(this.state_, State.OFFER_RESTORE);
  },

  isErrorLogsHidden_(state) {
    return !(
        this.isState_(this.state_, State.UPGRADE_ERROR) ||
        this.isState_(this.state_, State.OFFER_RESTORE));
  },

  /**
   * @param {State} state
   * @return {boolean}
   * @private
   */
  canDoAction_(state) {
    switch (state) {
      case State.PROMPT:
      case State.PRECHECKS_FAILED:
      case State.SUCCEEDED:
      case State.OFFER_RESTORE:
      case State.RESTORE_SUCCEEDED:
        return true;
    }
    return false;
  },

  /**
   * @param {State} state
   * @return {boolean}
   * @private
   */
  canCancel_(state) {
    switch (state) {
      case State.BACKUP:
      case State.RESTORE:
      case State.BACKUP_SUCCEEDED:
      case State.CANCELING:
      case State.SUCCEEDED:
        return false;
    }
    return true;
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_() {
    let titleId;
    switch (this.state_) {
      case State.PROMPT:
        titleId = 'promptTitle';
        break;
      case State.BACKUP:
        titleId = 'backingUpTitle';
        break;
      case State.BACKUP_SUCCEEDED:
        titleId = 'backupSucceededTitle';
        break;
      case State.PRECHECKS_FAILED:
        titleId = 'prechecksFailedTitle';
        break;
      case State.UPGRADING:
        titleId = 'upgradingTitle';
        break;
      case State.OFFER_RESTORE:
      case State.UPGRADE_ERROR:
      case State.ERROR:
        titleId = 'errorTitle';
        break;
      case State.RESTORE:
        titleId = 'restoreTitle';
        break;
      case State.RESTORE_SUCCEEDED:
        titleId = 'restoreSucceededTitle';
        break;
      case State.CANCELING:
        titleId = 'cancelingTitle';
        break;
      case State.SUCCEEDED:
        titleId = 'succeededTitle';
        break;
      default:
        assertNotReached();
    }
    return loadTimeData.getString(/** @type {string} */ (titleId));
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getActionButtonLabel_(state) {
    switch (state) {
      case State.PROMPT:
        return loadTimeData.getString('upgrade');
      case State.PRECHECKS_FAILED:
        return loadTimeData.getString('retry');
      case State.UPGRADE_ERROR:
      case State.ERROR:
        return loadTimeData.getString('cancel');
      case State.SUCCEEDED:
      case State.RESTORE_SUCCEEDED:
        return loadTimeData.getString('done');
      case State.OFFER_RESTORE:
        return loadTimeData.getString('restore');
    }
    return '';
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getCancelButtonLabel_(state) {
    switch (state) {
      case State.SUCCEEDED:
      case State.RESTORE_SUCCEEDED:
        return loadTimeData.getString('close');
      case State.PROMPT:
        return loadTimeData.getString('notNow');
      default:
        return loadTimeData.getString('cancel');
    }
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getProgressMessage_(state) {
    let messageId = null;
    switch (state) {
      case State.PROMPT:
        messageId = 'promptMessage';
        break;
      case State.BACKUP:
        messageId = 'backingUpMessage';
        break;
      case State.BACKUP_SUCCEEDED:
        messageId = 'backupSucceededMessage';
        break;
      case State.PRECHECKS_FAILED:
        switch (this.precheckStatus_) {
          case chromeos.crostiniUpgrader.mojom.UpgradePrecheckStatus
              .NETWORK_FAILURE:
            messageId = 'precheckNoNetwork';
            break;
          case chromeos.crostiniUpgrader.mojom.UpgradePrecheckStatus.LOW_POWER:
            messageId = 'precheckNoPower';
            break;
          case chromeos.crostiniUpgrader.mojom.UpgradePrecheckStatus
              .INSUFFICIENT_SPACE:
            messageId = 'precheckNoSpace';
            break;
          default:
            assertNotReached();
        }
        break;
      case State.UPGRADING:
        messageId = 'upgradingMessage';
        break;
      case State.RESTORE:
        messageId = 'restoreMessage';
        break;
      case State.RESTORE_SUCCEEDED:
        messageId = 'restoreSucceededMessage';
        break;
      case State.SUCCEEDED:
        messageId = 'succeededMessage';
        break;
    }
    return messageId ? loadTimeData.getString(messageId) : '';
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getErrorLogs_(state) {
    return this.progressMessages_.join('\n');
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getIllustrationStyle_(state) {
    switch (state) {
      case State.BACKUP_SUCCEEDED:
      case State.RESTORE_SUCCEEDED:
      case State.PRECHECKS_FAILED:
        return 'img-square-illustration';
      case State.OFFER_RESTORE:
      case State.UPGRADE_ERROR:
      case State.ERROR:
        return 'img-square-error-illustration';
    }
    return 'img-rect-illustration';
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getIllustrationURI_(state) {
    switch (state) {
      case State.BACKUP_SUCCEEDED:
      case State.RESTORE_SUCCEEDED:
        return 'images/success_illustration.svg';
      case State.PRECHECKS_FAILED:
      case State.OFFER_RESTORE:
      case State.UPGRADE_ERROR:
      case State.ERROR:
        return 'images/error_illustration.png';
    }
    return 'images/linux_illustration.png';
  },

  /**
   * @param {State} state
   * @return {boolean}
   * @private
   */
  hideIllustration_(state) {
    switch (state) {
      case State.BACKUP:
      case State.UPGRADING:
        return true;
    }
    return false;
  },

  /** @private */
  updateProgressLine_() {
    if (this.progressLineNumber_ < this.upgradeProgress_) {
      this.lastProgressLine_ =
          this.progressMessages_[this.progressLineNumber_++];
      var t = setTimeout(
          this.updateProgressLine_.bind(this), this.progressLineDisplayMs_);
    }
  },
});
