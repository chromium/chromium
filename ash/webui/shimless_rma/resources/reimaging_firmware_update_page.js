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

/** @type {!Object<!UpdateRoFirmwareStatus, string>} */
const updateRoFirmwareStatusTextKeys = {
  [UpdateRoFirmwareStatus.kWaitUsb]: 'firmwareUpdateWaitForUsbText',
  [UpdateRoFirmwareStatus.kFileNotFound]: 'firmwareUpdateFileNotFoundText',
  [UpdateRoFirmwareStatus.kUpdating]: 'firmwareUpdatingText',
  [UpdateRoFirmwareStatus.kRebooting]: 'firmwareUpdateRebootText',
  [UpdateRoFirmwareStatus.kComplete]: 'firmwareUpdateCompleteText',
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
      /** @protected {!UpdateRoFirmwareStatus} */
      status_: {
        type: Object,
      },

      /** @protected */
      statusString_: {
        type: String,
        computed: 'computeStatusString_(status_)',
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

  /**
   * @protected
   * @return {string}
   */
  computeStatusString_() {
    // kDownloading state is not used in V1.
    if (!this.status_ || this.status_ == UpdateRoFirmwareStatus.kDownloading) {
      return '';
    }
    return this.i18n(updateRoFirmwareStatusTextKeys[this.status_]);
  }

  /**
   * Implements UpdateRoFirmwareObserver.onUpdateRoFirmwareStatusChanged()
   * TODO(joonbug): Add error handling and display failure using cr-dialog.
   * @protected
   * @param {!UpdateRoFirmwareStatus} status
   */
  onUpdateRoFirmwareStatusChanged(status) {
    this.status_ = status;
    const disabled = this.status_ != UpdateRoFirmwareStatus.kComplete;
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: disabled},
        ));
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.status_ == UpdateRoFirmwareStatus.kComplete) {
      return this.shimlessRmaService_.roFirmwareUpdateComplete();
    } else {
      return Promise.reject(new Error('RO Firmware update is not complete.'));
    }
  }
}

customElements.define(UpdateRoFirmwarePage.is, UpdateRoFirmwarePage);
