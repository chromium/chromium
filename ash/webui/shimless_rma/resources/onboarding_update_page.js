// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_update_page.html.js';
import {HardwareVerificationStatusObserverInterface, HardwareVerificationStatusObserverReceiver, OsUpdateObserverInterface, OsUpdateObserverReceiver, OsUpdateOperation, ShimlessRmaServiceInterface, StateResult, UpdateErrorCode} from './shimless_rma.mojom-webui.js';
import {disableAllButtons, enableAllButtons, enableNextButton, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'onboarding-update-page' is the page shown when there is an Chrome OS update
 * available on the device for the user to install before the RMA process.
 */

const operationNameKeys = {
  [OsUpdateOperation.kIdle]: 'onboardingUpdateIdle',
  [OsUpdateOperation.kCheckingForUpdate]: 'onboardingUpdateChecking',
  [OsUpdateOperation.kUpdateAvailable]: 'onboardingUpdateAvailable',
  [OsUpdateOperation.kDownloading]: 'onboardingUpdateDownloading',
  [OsUpdateOperation.kVerifying]: 'onboardingUpdateVerifying',
  [OsUpdateOperation.kFinalizing]: 'onboardingUpdateFinalizing',
  [OsUpdateOperation.kUpdatedNeedReboot]: 'onboardingUpdateReboot',
  [OsUpdateOperation.kReportingErrorEvent]: 'onboardingUpdateError',
  [OsUpdateOperation.kAttemptingRollback]: 'onboardingUpdateRollback',
  [OsUpdateOperation.kDisabled]: 'onboardingUpdateDisabled',
  [OsUpdateOperation.kNeedPermissionToUpdate]: 'onboardingUpdatePermission',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingUpdatePageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingUpdatePageElement extends
    OnboardingUpdatePageElementBase {
  static get is() {
    return 'onboarding-update-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,

      /** @protected */
      currentVersionText: {
        type: String,
        value: '',
      },

      /** @protected */
      updateVersionButtonLabel: {
        type: String,
        value: '',
      },

      /** @protected */
      updateInProgress: {
        type: Boolean,
        value: false,
        observer:
            OnboardingUpdatePageElement.prototype.onUpdateInProgressChange,
      },

      /** @protected */
      verificationFailedMessage: {
        type: String,
        value: '',
      },

      /**
       * A string containing a list of the unqualified component identifiers
       * separated by new lines.
       * @protected
       */
      unqualifiedComponentsText: {
        type: String,
        value: '',
      },


      /** @protected */
      osUpdateEncounteredError: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }

    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();
    /** @protected {string} */
    this.currentVersion = '';
    /** @protected {?OsUpdateObserverReceiver} */
    this.osUpdateObserverReceiver = new OsUpdateObserverReceiver(
        /**
         * @type {!OsUpdateObserverInterface}
         */
        (this));

    this.shimlessRmaService.observeOsUpdateProgress(
        this.osUpdateObserverReceiver.$.bindNewPipeAndPassRemote());

    // We assume it's compliant until updated in onHardwareVerificationResult().
    this.isCompliant = true;
    /** @protected {?HardwareVerificationStatusObserverReceiver} */
    this.hwVerificationObserverReceiver =
        new HardwareVerificationStatusObserverReceiver(
            /**
             * @type {!HardwareVerificationStatusObserverInterface}
             */
            (this));

    this.shimlessRmaService.observeHardwareVerificationStatus(
        this.hwVerificationObserverReceiver.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.getCurrentVersionText();
    this.getUpdateVersionNumber();
    enableNextButton(this);

    focusPageTitle(this);
  }

  /**
   * @private
   */
  getCurrentVersionText() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.shimlessRmaService.getCurrentOsVersion().then((res) => {
      if (res.version != null) {
        this.currentVersion = res.version;
      } else {
        this.currentVersion = '0.0.0.0';
      }
      this.currentVersionText =
          this.i18n('currentVersionOutOfDateText', this.currentVersion);
    });
  }

  /** @private */
  getUpdateVersionNumber() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.shimlessRmaService.checkForOsUpdates().then((res) => {
      assert(res.updateAvailable);
      this.updateVersionButtonLabel =
          this.i18n('updateVersionRestartLabel', res.version);
    });
  }

  /** @private */
  updateOs() {
    this.updateInProgress = true;
    this.shimlessRmaService.updateOs().then((res) => {
      if (!res.updateStarted) {
        this.updateInProgress = false;
      }
    });
  }

  /** @protected */
  onUpdateButtonClicked() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }

    this.updateOs();
  }

  /** @protected */
  onRetryUpdateButtonClicked() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }

    assert(this.osUpdateEncounteredError);
    this.osUpdateEncounteredError = false;

    this.updateOs();
  }

  /** @return {!Promise<{stateResult: !StateResult}>} */
  onNextButtonClick() {
    return this.shimlessRmaService.updateOsSkipped();
  }

  /**
   * Implements OsUpdateObserver.onOsUpdateProgressUpdated()
   * @param {!OsUpdateOperation} operation
   * @param {number} progress
   * @param {UpdateErrorCode} error
   */
  onOsUpdateProgressUpdated(operation, progress, error) {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    // Ignore progress when not updating, it is just the update available check.
    if (!this.updateInProgress) {
      return;
    }

    if (operation === OsUpdateOperation.kIdle ||
        operation === OsUpdateOperation.kReportingErrorEvent ||
        operation === OsUpdateOperation.kNeedPermissionToUpdate ||
        operation === OsUpdateOperation.kDisabled) {
      this.updateInProgress = false;

      if (error !== UpdateErrorCode.kSuccess) {
        this.osUpdateEncounteredError = true;
      }
    }
  }

  /**
   * Implements
   * HardwareVerificationStatusObserver.onHardwareVerificationResult()
   * @param {boolean} isCompliant
   * @param {string} errorMessage
   */
  onHardwareVerificationResult(isCompliant, errorMessage) {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.isCompliant = isCompliant;

    if (!this.isCompliant) {
      this.unqualifiedComponentsText = errorMessage;
      this.setVerificationFailedMessage();
    }
  }

  /** @private */
  setVerificationFailedMessage() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.verificationFailedMessage = this.i18nAdvanced(
        'osUpdateUnqualifiedComponentsTopText', {attrs: ['id']});

    // The #unqualifiedComponentsLink identifier is sourced from the string
    // attached to `osUpdateUnqualifiedComponentsTopText` in the related .grd
    // file.
    const linkElement =
        this.shadowRoot.querySelector('#unqualifiedComponentsLink');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener(
        'click',
        () => this.shadowRoot.querySelector('#unqualifiedComponentsDialog')
                  .showModal());
  }

  /** @private */
  closeDialog() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.shadowRoot.querySelector('#unqualifiedComponentsDialog').close();
  }

  /** @private */
  onUpdateInProgressChange() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    if (this.updateInProgress) {
      disableAllButtons(this, /*showBusyStateOverlay=*/ false);
    } else {
      enableAllButtons(this);
    }
  }

  /**
   * @return {boolean}
   * @protected
   */
  shouldShowUpdateInstructions() {
    return !this.updateInProgress && !this.osUpdateEncounteredError;
  }
}

customElements.define(
    OnboardingUpdatePageElement.is, OnboardingUpdatePageElement);
