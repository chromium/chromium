// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_firmware_update_page.html.js';
import {ExternalDiskStateObserverInterface, ExternalDiskStateObserverReceiver, ShimlessRmaServiceInterface, StateResult, UpdateRoFirmwareObserverInterface, UpdateRoFirmwareObserverReceiver, UpdateRoFirmwareStatus} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

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
    return getTemplate();
  }

  static get properties() {
    return {
      /** @protected {?UpdateRoFirmwareStatus} */
      status: {
        type: Object,
        value: null,
      },

      /** @protected {string} */
      statusString: {
        type: String,
      },

      /** @protected {boolean} */
      shouldShowSpinner: {
        type: Boolean,
        value: false,
      },

      /** @protected {boolean} */
      shouldShowWarning: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @protected {string} */
      imgSrc: {
        type: String,
        value: '',
      },

      /** @protected {string} */
      imgAlt: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService = getShimlessRmaService();
    /** @private {UpdateRoFirmwareObserverReceiver} */
    this.updateRoFirmwareObserverReceiver =
        new UpdateRoFirmwareObserverReceiver(
            /**
             * @type {!UpdateRoFirmwareObserverInterface}
             */
            (this));

    this.shimlessRmaService.observeRoFirmwareUpdateProgress(
        this.updateRoFirmwareObserverReceiver.$.bindNewPipeAndPassRemote());

    /** @private {!ExternalDiskStateObserverReceiver} */
    this.externalDiskStateReceiver = new ExternalDiskStateObserverReceiver(
        /** @type {!ExternalDiskStateObserverInterface} */ (this));

    this.shimlessRmaService.observeExternalDiskState(
        this.externalDiskStateReceiver.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  static get observers() {
    return ['onStatusChanged(status)'];
  }

  /**
   * Implements UpdateRoFirmwareObserver.onUpdateRoFirmwareStatusChanged()
   * @param {!UpdateRoFirmwareStatus} status
   * @protected
   */
  onUpdateRoFirmwareStatusChanged(status) {
    this.status = status;
    this.shouldShowSpinner = this.status === UpdateRoFirmwareStatus.kUpdating;
    this.shouldShowWarning =
        this.status === UpdateRoFirmwareStatus.kFileNotFound;
  }

  /**
   * Implements ExternalDiskStateObserver.onExternalDiskStateChanged()
   * @param {boolean} detected
   */
  onExternalDiskStateChanged(detected) {
    if (!detected && this.status === UpdateRoFirmwareStatus.kComplete) {
      executeThenTransitionState(
          this, () => this.shimlessRmaService.roFirmwareUpdateComplete());
    }
  }

  /**
   * Groups state changes related to the |status| updating.
   * @protected
   */
  onStatusChanged() {
    this.setStatusString();
    this.setImgSrcAndAlt();
  }

  /**
   * @protected
   */
  setStatusString() {
    this.statusString =
        !this.status ? '' : this.i18n(STATUS_TEXT_KEY_MAP[this.status]);
  }

  /**
   * @protected
   */
  setImgSrcAndAlt() {
    this.imgSrc = `illustrations/${
    !this.status ? 'downloading' : STATUS_IMG_MAP[this.status]}.svg`;
    this.imgAlt = this.i18n(
        !this.status ? 'downloadingAltText' : STATUS_ALT_MAP[this.status]);
  }

  /**
   * @return {string}
   * @protected
   */
  getTitleText() {
    return this.i18n(
        this.status === UpdateRoFirmwareStatus.kComplete ?
            'firmwareUpdateInstallCompleteTitleText' :
            'firmwareUpdateInstallImageTitleText');
  }
}

customElements.define(UpdateRoFirmwarePage.is, UpdateRoFirmwarePage);
