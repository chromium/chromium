// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {OsUpdateObserverInterface, OsUpdateObserverReceiver, OsUpdateOperation, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

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

export class OnboardingUpdatePageElement extends PolymerElement {
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
      checkInProgress_: {
        type: Boolean,
        value: false,
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
  }

  /** @override */
  ready() {
    super.ready();
    this.getCurrentVersionText_();
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
      this.currentVersionText_ = `Current version ${this.currentVersion_}`;
    });
    // TODO(joonbug): i18n string
  }

  /** @protected */
  onUpdateCheckButtonClicked_() {
    this.checkInProgress_ = true;
    this.shimlessRmaService_.checkForOsUpdates().then((res) => {
      if (res.updateAvailable) {
        this.updateAvailable_ = true;
        // TODO(joonbug): i18n string
        this.currentVersionText_ =
            `Current version ${this.currentVersion_} is out of date`;
      } else {
        // TODO(joonbug): i18n string
        this.currentVersionText_ =
            `Current version ${this.currentVersion_} is up to date`;
      }
      this.checkInProgress_ = false;
    });
  }

  /** @protected */
  onUpdateButtonClicked_() {
    this.updateInProgress_ = true;
    this.shimlessRmaService_.updateOs().then((res) => {
      if (!res.updateStarted) {
        // TODO(gavindodd): i18n string
        this.updateProgressMessage_ = 'OS update failed';
        this.updateInProgress_ = false;
      }
    });
  }

  /**
   * @protected
   */
  updateCheckButtonHidden_() {
    return !this.networkAvailable || this.updateAvailable_;
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
    if (operation === OsUpdateOperation.kIdle) {
      this.updateInProgress_ = false;
    }
    // TODO(gavindodd): i18n string
    this.updateProgressMessage_ = 'OS update progress received ' +
        operationName[operation] + ' ' + Math.round(progress * 100) + '%';
  }
};

customElements.define(
    OnboardingUpdatePageElement.is, OnboardingUpdatePageElement);
