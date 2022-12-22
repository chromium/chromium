// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareVerificationStatusObserverInterface, HardwareVerificationStatusObserverReceiver, OsUpdateObserverInterface, OsUpdateObserverReceiver, OsUpdateOperation, ShimlessRmaServiceInterface, StateResult, UpdateErrorCode} from './shimless_rma_types.js';
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
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,

      /** @protected */
      currentVersionText_: {
        type: String,
        value: '',
      },

      /** @protected */
      updateVersionButtonLabel_: {
        type: String,
        value: '',
      },

      /** @protected */
      updateInProgress_: {
        type: Boolean,
        value: false,
        observer:
            OnboardingUpdatePageElement.prototype.onUpdateInProgressChange_,
      },

      /** @protected */
      verificationFailedMessage_: {
        type: String,
        value: '',
      },

      /**
       * A string containing a list of the unqualified component identifiers
       * separated by new lines.
       * @protected
       */
      unqualifiedComponentsText_: {
        type: String,
        value: '',
      },


      /** @protected */
      osUpdateEncounteredError_: {
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
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @protected {string} */
    this.currentVersion_ = '';
    /** @protected {?OsUpdateObserverReceiver} */
    this.osUpdateObserverReceiver_ = new OsUpdateObserverReceiver(
      /**
       * @type {!OsUpdateObserverInterface}
       */
      (this));

    this.shimlessRmaService_.observeOsUpdateProgress(
        this.osUpdateObserverReceiver_.$.bindNewPipeAndPassRemote());

    // We assume it's compliant until updated in onHardwareVerificationResult().
    this.isCompliant_ = true;
    /** @protected {?HardwareVerificationStatusObserverReceiver} */
    this.hwVerificationObserverReceiver_ =
        new HardwareVerificationStatusObserverReceiver(
            /**
             * @type {!HardwareVerificationStatusObserverInterface}
             */
            (this));

    this.shimlessRmaService_.observeHardwareVerificationStatus(
        this.hwVerificationObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.getCurrentVersionText_();
    this.getUpdateVersionNumber_();
    enableNextButton(this);

    focusPageTitle(this);
  }

  /**
   * @private
   */
  getCurrentVersionText_() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.shimlessRmaService_.getCurrentOsVersion().then((res) => {
      if (res.version != null) {
        this.currentVersion_ = res.version;
      } else {
        this.currentVersion_ = '0.0.0.0';
      }
      this.currentVersionText_ =
          this.i18n('currentVersionOutOfDateText', this.currentVersion_);
    });
  }

  /** @private */
  getUpdateVersionNumber_() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.shimlessRmaService_.checkForOsUpdates().then((res) => {
      assert(res.updateAvailable);
      this.updateVersionButtonLabel_ =
          this.i18n('updateVersionRestartLabel', res.version);
    });
  }

  /** @private */
  updateOs_() {
    this.updateInProgress_ = true;
    this.shimlessRmaService_.updateOs().then((res) => {
      if (!res.updateStarted) {
        this.updateInProgress_ = false;
      }
    });
  }

  /** @protected */
  onUpdateButtonClicked_() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }

    this.updateOs_();
  }

  /** @protected */
  onRetryUpdateButtonClicked_() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }

    assert(this.osUpdateEncounteredError_);
    this.osUpdateEncounteredError_ = false;

    this.updateOs_();
  }

  /** @return {!Promise<{stateResult: !StateResult}>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.updateOsSkipped();
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
    if (!this.updateInProgress_) {
      return;
    }

    if (operation === OsUpdateOperation.kIdle ||
        operation === OsUpdateOperation.kReportingErrorEvent ||
        operation === OsUpdateOperation.kNeedPermissionToUpdate ||
        operation === OsUpdateOperation.kDisabled) {
      this.updateInProgress_ = false;

      if (error !== UpdateErrorCode.kSuccess) {
        this.osUpdateEncounteredError_ = true;
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
    this.isCompliant_ = isCompliant;

    if (!this.isCompliant_) {
      this.unqualifiedComponentsText_ = errorMessage;
      this.setVerificationFailedMessage_();
    }
  }

  /** @private */
  setVerificationFailedMessage_() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.verificationFailedMessage_ = this.i18nAdvanced(
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
  closeDialog_() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    this.shadowRoot.querySelector('#unqualifiedComponentsDialog').close();
  }

  /** @private */
  onUpdateInProgressChange_() {
    if (!loadTimeData.getBoolean('osUpdateEnabled')) {
      return;
    }
    if (this.updateInProgress_) {
      disableAllButtons(this, /*showBusyStateOverlay=*/ false);
    } else {
      enableAllButtons(this);
    }
  }

  /**
   * @return {boolean}
   * @protected
   */
  shouldShowUpdateInstructions_() {
    return !this.updateInProgress_ && !this.osUpdateEncounteredError_;
  }
}

customElements.define(
    OnboardingUpdatePageElement.is, OnboardingUpdatePageElement);
