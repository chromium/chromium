// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareVerificationStatusObserverInterface, HardwareVerificationStatusObserverReceiver, OsUpdateObserverInterface, OsUpdateObserverReceiver, OsUpdateOperation, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableAllButtons, enableAllButtons, enableNextButton} from './shimless_rma_util.js';

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
  [OsUpdateOperation.kNeedPermissionToUpdate]: 'onboardingUpdatePermission'
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

      /**
       * TODO(joonbug): populate this and make private.
       */
      networkAvailable: {
        type: Boolean,
        value: true,
      },

      /** @protected */
      updateInProgress_: {
        type: Boolean,
        value: false,
        observer: 'onUpdateInProgressChange_',
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
    };
  }

  constructor() {
    super();
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
    this.getCurrentVersionText_();
    this.getUpdateVersionNumber_();
    enableNextButton(this);
  }

  /**
   * @private
   */
  getCurrentVersionText_() {
    this.shimlessRmaService_.getCurrentOsVersion().then((res) => {
      this.currentVersion_ = res.version;
      this.currentVersionText_ =
          this.i18n('currentVersionOutOfDateText', this.currentVersion_);
    });
  }

  /** @private */
  getUpdateVersionNumber_() {
    this.shimlessRmaService_.checkForOsUpdates().then((res) => {
      assert(res.updateAvailable);
      this.updateVersionButtonLabel_ =
          this.i18n('updateVersionRestartLabel', res.version);
    });
  }

  /** @protected */
  onUpdateButtonClicked_() {
    this.updateInProgress_ = true;
    this.shimlessRmaService_.updateOs().then((res) => {
      if (!res.updateStarted) {
        this.updateInProgress_ = false;
      }
    });
  }

  /** @return {!Promise<StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.updateOsSkipped();
  }

  /**
   * Implements OsUpdateObserver.onOsUpdateProgressUpdated()
   * @param {!OsUpdateOperation} operation
   * @param {number} progress
   */
  onOsUpdateProgressUpdated(operation, progress) {
    // Ignore progress when not updating, it is just the update available check.
    if (!this.updateInProgress_) {
      return;
    }

    if (operation === OsUpdateOperation.kIdle ||
        operation === OsUpdateOperation.kReportingErrorEvent ||
        operation === OsUpdateOperation.kNeedPermissionToUpdate ||
        operation === OsUpdateOperation.kDisabled) {
      this.updateInProgress_ = false;
    }
  }

  /**
   * Implements
   * HardwareVerificationStatusObserver.onHardwareVerificationResult()
   * @param {boolean} isCompliant
   * @param {string} errorMessage
   */
  onHardwareVerificationResult(isCompliant, errorMessage) {
    this.isCompliant_ = isCompliant;

    if (!this.isCompliant_) {
      this.unqualifiedComponentsText_ = errorMessage;
      this.setVerificationFailedMessage_();
    }
  }

  /** @private */
  setVerificationFailedMessage_() {
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
    this.shadowRoot.querySelector('#unqualifiedComponentsDialog').close();
  }

  /** @private */
  onUpdateInProgressChange_() {
    if (this.updateInProgress_) {
      disableAllButtons(this, /*showBusyStateOverlay=*/ false);
    } else {
      enableAllButtons(this);
    }
  }
}

customElements.define(
    OnboardingUpdatePageElement.is, OnboardingUpdatePageElement);
