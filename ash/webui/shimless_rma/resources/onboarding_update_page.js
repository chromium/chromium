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

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareVerificationStatusObserverInterface, HardwareVerificationStatusObserverReceiver, OsUpdateObserverInterface, OsUpdateObserverReceiver, OsUpdateOperation, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'onboarding-update-page' is the page that checks to see if the version is up
 * to date before starting the rma process.
 */

// TODO(gavindodd): i18n string
const operationName = {
  [OsUpdateOperation.kIdle]: 'Not updating',
  [OsUpdateOperation.kCheckingForUpdate]: 'checking for update',
  [OsUpdateOperation.kUpdateAvailable]: 'update found',
  [OsUpdateOperation.kDownloading]: 'downloading update',
  [OsUpdateOperation.kVerifying]: 'verifying update',
  [OsUpdateOperation.kFinalizing]: 'installing update',
  [OsUpdateOperation.kUpdatedNeedReboot]: 'need reboot',
  [OsUpdateOperation.kReportingErrorEvent]: 'error updating',
  [OsUpdateOperation.kAttemptingRollback]: 'attempting rollback',
  [OsUpdateOperation.kDisabled]: 'update disabled',
  [OsUpdateOperation.kNeedPermissionToUpdate]: 'need permission to update'
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
      /** @protected */
      currentVersionText_: {
        type: String,
        value: '',
      },

      /** @protected */
      updateNoticeMessage_: {
        type: String,
        value: '',
      },

      /** @protected */
      updateProgressMessage_: {
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
      },

      /** @protected */
      updateAvailable_: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      updateVersion_: {
        type: String,
        value: '',
      }
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

    this.isCompliant_ = false;
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
    this.checkForUdpates_();
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }

  /**
   * @private
   */
  getCurrentVersionText_() {
    this.shimlessRmaService_.getCurrentOsVersion().then((res) => {
      this.currentVersion_ = res.version;
      this.currentVersionText_ =
          this.i18n('currentVersionText', this.currentVersion_);
    });
  }

  /** @private */
  checkForUdpates_() {
    this.shimlessRmaService_.checkForOsUpdates().then((res) => {
      if (res && res.updateAvailable) {
        this.updateAvailable_ = true;
        this.updateVersion_ = res.version;
      }

      this.currentVersionText_ = this.i18n(
          this.updateAvailable_ ? 'currentVersionOutOfDateText' :
                                  'currentVersionUpToDateText',
          this.currentVersion_);
      this.setUpdateNoticeMessage_();
    });
  }

  /** @protected */
  onUpdateButtonClicked_() {
    this.updateInProgress_ = true;
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
    this.shimlessRmaService_.updateOs().then((res) => {
      if (!res.updateStarted) {
        // TODO(gavindodd): i18n string
        this.updateProgressMessage_ = 'OS update failed';
        this.updateInProgress_ = false;
        this.dispatchEvent(new CustomEvent(
            'disable-next-button',
            {bubbles: true, composed: true, detail: false},
            ));
      }
    });
  }

  /**
   * @protected
   * @return {string}
   */
  getUpdateNoticeIcon_() {
    return this.updateAvailable_ ? 'shimless-icon:info' : 'shimless-icon:check';
  }

  /** @protected */
  updateCheckButtonHidden_() {
    return !this.networkAvailable || this.updateAvailable_;
  }

  /** @private */
  setUpdateNoticeMessage_() {
    // TODO(gavindodd): i18n string
    if (!this.isCompliant_) {
      this.updateNoticeMessage_ = 'An unrecognized component has been found. ' +
          'Unrecognized devices will not be configured correctly and may be ' +
          'unusable. Updating to the latest version of Chrome OS may resolve ' +
          'this issue.';
    } else if (this.updateAvailable_) {
      // TODO(gavindodd): Do we need a check that the current major version is
      // within n of the installed version to switch between this message and
      // 'Chrome OS needs an additional update to get fully up to date.'?
      this.updateNoticeMessage_ = 'If Chrome OS is out of date, Shimless RMA ' +
          'process may have been updated since this version was installed.';
    } else {
      // Note: In current implementation this should not be reached, but it is
      // still a perfectly valid state.
      // If there was ever an update that did not require a reboot this would
      // be reached.
      this.updateNoticeMessage_ = '';
    }
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
      this.dispatchEvent(new CustomEvent(
          'disable-next-button',
          {bubbles: true, composed: true, detail: false},
          ));
    }
    // TODO(gavindodd): i18n string
    this.updateProgressMessage_ = 'OS update progress received ' +
        operationName[operation] + ' ' + Math.round(progress * 100) + '%';
  }

  /**
   * Implements
   * HardwareVerificationStatusObserver.onHardwareVerificationResult()
   * @param {boolean} isCompliant
   * @param {string} errorMessage
   */
  onHardwareVerificationResult(isCompliant, errorMessage) {
    this.isCompliant_ = isCompliant;
    this.setUpdateNoticeMessage_();
  }
}

customElements.define(
    OnboardingUpdatePageElement.is, OnboardingUpdatePageElement);
