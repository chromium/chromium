// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import '/strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {UpgradePrecheckStatus} from './crostini_upgrader.mojom-webui.js';

/**
 * Enum for the state of `crostini-upgrader-app`.
 */
enum State {
  PROMPT = 'prompt',
  BACKUP = 'backup',
  BACKUP_ERROR = 'backupError',
  BACKUP_SUCCEEDED = 'backupSucceeded',
  PRECHECKS_FAILED = 'prechecksFailed',
  UPGRADING = 'upgrading',
  UPGRADE_ERROR = 'upgrade_error',
  OFFER_RESTORE = 'offerRestore',
  RESTORE = 'restore',
  RESTORE_ERROR = 'restoreError',
  RESTORE_SUCCEEDED = 'restoreSucceeded',
  CANCELING = 'canceling',
  SUCCEEDED = 'succeeded',
}

const kMaxUpgradeAttempts: number = 3;


class CrostiniUpgraderAppElement extends PolymerElement {
  static get is() {
    return 'crostini-upgrader-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      state_: {
        type: String,
        value: State.PROMPT,
      },

      backupCheckboxChecked_: {
        type: Boolean,
        value: true,
      },

      backupProgress_: {
        type: Number,
      },

      upgradeProgress_: {
        type: Number,
        value: 0,
      },

      restoreProgress_: {
        type: Number,
      },

      progressMessages_: {
        type: Array,
        value: () => [],
      },

      progressLineNumber_: {
        type: Number,
        value: 0,
      },

      lastProgressLine_: {
        type: String,
        value: '',
      },

      progressLineDisplayMs_: {
        type: Number,
        value: 300,
      },

      upgradeAttemptCount_: {
        type: Number,
        value: 0,
      },

      logFileName_: {
        type: String,
        value: '',
      },

      precheckStatus_: {
        type: Number,
        value: UpgradePrecheckStatus.OK,
      },

      /**
       * Enable the html template to use State.
       */
      stateEnum_: {
        type: Object,
        value: State,
      },
    };
  }

  private state_: State;
  private backupCheckboxChecked_: boolean;
  private backupProgress_: number;
  private upgradeProgress_: number;
  private restoreProgress_: number;
  private progressMessages_: string[];
  private progressLineNumber_: number;
  private lastProgressLine_: string;
  private progressLineDisplayMs_: number;
  private upgradeAttemptCount_: number;
  private logFileName_: string;
  private precheckStatus_: UpgradePrecheckStatus;

  private listenerIds_: number[] = [];
  private precheckSuccessCallback_: () => void;
  private precheckFailureCallback_: () => void;

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;

    this.listenerIds_ = [
      callbackRouter.onBackupProgress.addListener((percent: number) => {
        this.state_ = State.BACKUP;
        this.backupProgress_ = percent;
      }),
      callbackRouter.onBackupSucceeded.addListener((wasCancelled: boolean) => {
        assert(this.state_ === State.BACKUP);
        this.state_ = State.BACKUP_SUCCEEDED;
        // We do a short (2 second) interstitial display of the backup success
        // message before continuing the upgrade.
        const timeout = new Promise((resolve, _reject) => {
          setTimeout(resolve, wasCancelled ? 0 : 2000);
        });
        // We also want to wait for the prechecks to finish.
        const callback = new Promise<void>((resolve, reject) => {
          this.startPrechecks_(resolve, reject);
        });
        Promise.all([timeout, callback]).then(() => {
          this.startUpgrade_();
        });
      }),
      callbackRouter.onBackupFailed.addListener(() => {
        assert(this.state_ === State.BACKUP);
        this.state_ = State.BACKUP_ERROR;
      }),
      callbackRouter.precheckStatus.addListener(
          (status: UpgradePrecheckStatus) => {
            if (status === UpgradePrecheckStatus.OK) {
              this.precheckSuccessCallback_();
              this.precheckStatus_ = status;
            } else {
              this.precheckStatus_ = status;
              this.state_ = State.PRECHECKS_FAILED;
              this.precheckFailureCallback_();
            }
          }),
      callbackRouter.onUpgradeProgress.addListener(
          (progressMessages: string[]) => {
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
      callbackRouter.onRestoreProgress.addListener((percent: number) => {
        assert(this.state_ === State.RESTORE);
        this.restoreProgress_ = percent;
      }),
      callbackRouter.onRestoreSucceeded.addListener(() => {
        assert(this.state_ === State.RESTORE);
        this.state_ = State.RESTORE_SUCCEEDED;
      }),
      callbackRouter.onRestoreFailed.addListener(() => {
        assert(this.state_ === State.RESTORE);
        this.state_ = State.RESTORE_ERROR;
      }),
      callbackRouter.onCanceled.addListener(() => {
        if (this.state_ === State.RESTORE) {
          this.state_ = State.RESTORE_ERROR;
          return;
        }
        this.closePage_();
      }),
      callbackRouter.requestClose.addListener(() => {
        if (this.canCancel_(this.state_)) {
          this.onCancelButtonClick_();
        }
      }),
      callbackRouter.onLogFileCreated.addListener((path: string) => {
        this.logFileName_ = path;
      }),
    ];

    document.addEventListener('keyup', event => {
      if (event.key === 'Escape' && this.canCancel_(this.state_)) {
        this.onCancelButtonClick_();
        event.preventDefault();
      }
    });

    this.shadowRoot!.querySelector<HTMLElement>('.action-button')!.focus();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => callbackRouter.removeListener(id));
  }

  private precheckThenUpgrade_() {
    this.startPrechecks_(() => {
      this.startUpgrade_();
    }, () => {});
  }

  private onActionButtonClick_() {
    switch (this.state_) {
      case State.SUCCEEDED:
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
      default:
        assertNotReached();
    }
  }

  private onCancelButtonClick_() {
    switch (this.state_) {
      case State.PROMPT:
        BrowserProxy.getInstance().handler.cancelBeforeStart();
        break;
      case State.UPGRADING:
        this.state_ = State.CANCELING;
        BrowserProxy.getInstance().handler.cancel();
        break;
      case State.RESTORE_SUCCEEDED:
        BrowserProxy.getInstance().handler.launch();
        this.closePage_();
        break;
      case State.PRECHECKS_FAILED:
      case State.BACKUP_ERROR:
      case State.UPGRADE_ERROR:
      case State.RESTORE_ERROR:
      case State.OFFER_RESTORE:
      case State.SUCCEEDED:
        this.closePage_();
        break;
      case State.CANCELING:
        break;
      default:
        assertNotReached();
    }
  }

  private onChangeLocationButtonClick_() {
    this.startBackup_(/*showFileChooser=*/ true);
  }

  private startBackup_(showFileChooser: boolean) {
    BrowserProxy.getInstance().handler.backup(showFileChooser);
  }

  private startPrechecks_(success: () => void, failure: () => void) {
    this.precheckSuccessCallback_ = success;
    this.precheckFailureCallback_ = failure;
    BrowserProxy.getInstance().handler.startPrechecks();
  }

  private startUpgrade_() {
    this.state_ = State.UPGRADING;
    this.upgradeAttemptCount_++;
    BrowserProxy.getInstance().handler.upgrade();
  }

  private startRestore_() {
    this.state_ = State.RESTORE;
    BrowserProxy.getInstance().handler.restore();
  }

  private closePage_() {
    BrowserProxy.getInstance().handler.onPageClosed();
  }

  private isState_(state1: State, state2: State): boolean {
    return state1 === state2;
  }

  private isErrorLogsHidden_(): boolean {
    return !(
        this.isState_(this.state_, State.UPGRADE_ERROR) ||
        this.isState_(this.state_, State.OFFER_RESTORE));
  }

  private canDoAction_(state: State): boolean {
    switch (state) {
      case State.PROMPT:
      case State.PRECHECKS_FAILED:
      case State.SUCCEEDED:
      case State.OFFER_RESTORE:
        return true;
    }
    return false;
  }

  private canCancel_(state: State): boolean {
    switch (state) {
      case State.BACKUP:
      case State.RESTORE:
      case State.BACKUP_SUCCEEDED:
      case State.CANCELING:
      case State.SUCCEEDED:
        return false;
    }
    return true;
  }

  private getTitle_(): string {
    let titleId: string = '';

    switch (this.state_) {
      case State.PROMPT:
        titleId = 'promptTitle';
        break;
      case State.BACKUP:
        titleId = 'backingUpTitle';
        break;
      case State.BACKUP_ERROR:
        titleId = 'backupErrorTitle';
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
        titleId = 'errorTitle';
        break;
      case State.RESTORE:
        titleId = 'restoreTitle';
        break;
      case State.RESTORE_ERROR:
        titleId = 'restoreErrorTitle';
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
    return loadTimeData.getString(titleId);
  }

  private getActionButtonLabel_(state: State): string {
    switch (state) {
      case State.PROMPT:
        return loadTimeData.getString('upgrade');
      case State.PRECHECKS_FAILED:
        return loadTimeData.getString('retry');
      case State.SUCCEEDED:
        return loadTimeData.getString('done');
      case State.OFFER_RESTORE:
        return loadTimeData.getString('restore');
    }
    return '';
  }

  private getCancelButtonLabel_(state: State): string {
    switch (state) {
      case State.RESTORE_SUCCEEDED:
      case State.BACKUP_ERROR:
      case State.UPGRADE_ERROR:
      case State.RESTORE_ERROR:
        return loadTimeData.getString('close');
      case State.PROMPT:
        return loadTimeData.getString('notNow');
      default:
        return loadTimeData.getString('cancel');
    }
  }

  private getProgressMessage_(
      state: State, precheckStatus: UpgradePrecheckStatus,
      fileName: string): TrustedHTML {
    let messageId = null;
    switch (state) {
      case State.PROMPT:
        messageId = 'promptMessage';
        break;
      case State.BACKUP:
        messageId = 'backingUpMessage';
        break;
      case State.BACKUP_ERROR:
        messageId = 'backupErrorMessage';
        break;
      case State.PRECHECKS_FAILED:
        switch (precheckStatus) {
          case UpgradePrecheckStatus.NETWORK_FAILURE:
            messageId = 'precheckNoNetwork';
            break;
          case UpgradePrecheckStatus.LOW_POWER:
            messageId = 'precheckNoPower';
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
      case State.RESTORE_ERROR:
        messageId = 'restoreErrorMessage';
        break;
      case State.SUCCEEDED:
        return sanitizeInnerHtml(
            loadTimeData.getStringF('logFileMessageSuccess', fileName));
      case State.UPGRADE_ERROR:
      case State.OFFER_RESTORE:
        return sanitizeInnerHtml(
            loadTimeData.getStringF('logFileMessageError', fileName));
    }
    return messageId ? sanitizeInnerHtml(loadTimeData.getString(messageId)) :
                       window.trustedTypes!.emptyHTML;
  }

  private getErrorLogs_(): string {
    return this.progressMessages_.join('\n');
  }

  private getIllustrationStyle_(state: State): string {
    switch (state) {
      case State.BACKUP_ERROR:
      case State.BACKUP_SUCCEEDED:
      case State.RESTORE_ERROR:
      case State.RESTORE_SUCCEEDED:
      case State.PRECHECKS_FAILED:
        return 'img-square-illustration';
    }
    return 'img-rect-illustration';
  }

  private getIllustrationURI_(state: State): string {
    switch (state) {
      case State.BACKUP_SUCCEEDED:
      case State.RESTORE_SUCCEEDED:
        return 'images/success_illustration.svg';
      case State.PRECHECKS_FAILED:
      case State.BACKUP_ERROR:
      case State.RESTORE_ERROR:
        return 'images/error_illustration.png';
    }
    return 'images/linux_illustration.png';
  }

  private hideIllustration_(state: State): boolean {
    switch (state) {
      case State.OFFER_RESTORE:
      case State.UPGRADE_ERROR:
        return true;
    }
    return false;
  }

  private updateProgressLine_() {
    if (this.progressLineNumber_ < this.upgradeProgress_) {
      this.lastProgressLine_ =
          this.progressMessages_[this.progressLineNumber_++];
      const t = setTimeout(
          this.updateProgressLine_.bind(this), this.progressLineDisplayMs_);
    }
  }
}

customElements.define('crostini-upgrader-app', CrostiniUpgraderAppElement);
