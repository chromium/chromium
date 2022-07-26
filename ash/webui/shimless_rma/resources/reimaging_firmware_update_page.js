// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared_css.js';
import './base_page.js';
import './icons.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult, UpdateRoFirmwareObserverInterface, UpdateRoFirmwareObserverReceiver, UpdateRoFirmwareStatus} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton} from './shimless_rma_util.js';

/** @type {!Object<!UpdateRoFirmwareStatus, string>} */
const STATUS_TEXT_KEY_MAP = {
  // kDownloading state is not used in V1.
  [UpdateRoFirmwareStatus.kDownloading]: '',
  [UpdateRoFirmwareStatus.kWaitUsb]: 'firmwareUpdateWaitForUsbText',
  [UpdateRoFirmwareStatus.kFileNotFound]: 'firmwareUpdateFileNotFoundText',
  [UpdateRoFirmwareStatus.kUpdating]: 'firmwareUpdatingText',
  [UpdateRoFirmwareStatus.kRebooting]: 'firmwareUpdateRebootText',
  [UpdateRoFirmwareStatus.kComplete]: 'firmwareUpdateCompleteText',
};

/** @type {!Object<!UpdateRoFirmwareStatus, string>} */
const STATUS_IMG_MAP = {
  [UpdateRoFirmwareStatus.kWaitUsb]: 'insert_usb',
  [UpdateRoFirmwareStatus.kFileNotFound]: 'error',
  [UpdateRoFirmwareStatus.kRebooting]: 'downloading',
  [UpdateRoFirmwareStatus.kComplete]: 'downloading',
};

/** @type {!Object<!UpdateRoFirmwareStatus, string>} */
const STATUS_ALT_MAP = {
  [UpdateRoFirmwareStatus.kWaitUsb]: 'insertUsbAltText',
  [UpdateRoFirmwareStatus.kFileNotFound]: 'errorAltText',
  [UpdateRoFirmwareStatus.kUpdating]: 'updateOsAltText',
  [UpdateRoFirmwareStatus.kRebooting]: 'downloadingAltText',
  [UpdateRoFirmwareStatus.kComplete]: 'downloadingAltText',
};

/**
 * @fileoverview
 * 'firmware-updating-page' displays status of firmware update.
 *
 * The kWaitUsb state requires the user to insert a USB with a recovery image.
 * If there is an error other than 'file not found' an error signal will be
 * received and handled by |ShimlessRma| and the status will return to kWaitUsb.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const UpdateRoFirmwarePageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class UpdateRoFirmwarePage extends UpdateRoFirmwarePageBase {
  static get is() {
    return 'reimaging-firmware-update-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected {?UpdateRoFirmwareStatus} */
      status_: {
        type: Object,
        value: null,
      },

      /** @protected {string} */
      statusString_: {
        type: String,
      },

      /** @protected {boolean} */
      shouldShowSpinner_: {
        type: Boolean,
        value: false,
      },

      /** @protected {boolean} */
      shouldShowWarning_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @protected {string} */
      imgSrc_: {
        type: String,
        value: '',
      },

      /** @protected {string} */
      imgAlt_: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @private {UpdateRoFirmwareObserverReceiver} */
    this.updateRoFirmwareObserverReceiver_ =
        new UpdateRoFirmwareObserverReceiver(
            /**
             * @type {!UpdateRoFirmwareObserverInterface}
             */
            (this));

    this.shimlessRmaService_.observeRoFirmwareUpdateProgress(
        this.updateRoFirmwareObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  static get observers() {
    return ['onStatusChanged_(status_)'];
  }

  /**
   * Implements UpdateRoFirmwareObserver.onUpdateRoFirmwareStatusChanged()
   * @param {!UpdateRoFirmwareStatus} status
   * @protected
   */
  onUpdateRoFirmwareStatusChanged(status) {
    this.status_ = status;
    this.shouldShowSpinner_ = this.status_ === UpdateRoFirmwareStatus.kUpdating;
    this.shouldShowWarning_ =
        this.status_ === UpdateRoFirmwareStatus.kFileNotFound;

    const disabled = this.status_ != UpdateRoFirmwareStatus.kComplete;
    if (disabled) {
      disableNextButton(this);
    } else {
      enableNextButton(this);
    }
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    if (this.status_ == UpdateRoFirmwareStatus.kComplete) {
      return this.shimlessRmaService_.roFirmwareUpdateComplete();
    } else {
      return Promise.reject(new Error('RO Firmware update is not complete.'));
    }
  }

  /**
   * Groups state changes related to the |status_| updating.
   * @protected
   */
  onStatusChanged_() {
    this.setStatusString_();
    this.setImgSrcAndAlt_();
  }

  /**
   * @protected
   */
  setStatusString_() {
    this.statusString_ =
        !this.status_ ? '' : this.i18n(STATUS_TEXT_KEY_MAP[this.status_]);
  }

  /**
   * @protected
   */
  setImgSrcAndAlt_() {
    this.imgSrc_ = `illustrations/${
    !this.status_ ? 'downloading' : STATUS_IMG_MAP[this.status_]}.svg`;
    this.imgAlt_ = this.i18n(
        !this.status_ ? 'downloadingAltText' : STATUS_ALT_MAP[this.status_]);
  }

  /**
   * @return {string}
   * @protected
   */
  getTitleText_() {
    return this.i18n(
        this.status_ === UpdateRoFirmwareStatus.kComplete ?
            'firmwareUpdateInstallCompleteTitleText' :
            'firmwareUpdateInstallImageTitleText');
  }
}

customElements.define(UpdateRoFirmwarePage.is, UpdateRoFirmwarePage);
