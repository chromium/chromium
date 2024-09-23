// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './shimless_rma_shared.css.js';
import './base_page.js';
import './icons.html.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_firmware_update_page.html.js';
import {ExternalDiskStateObserverReceiver, ShimlessRmaServiceInterface, UpdateRoFirmwareObserverReceiver, UpdateRoFirmwareStatus} from './shimless_rma.mojom-webui.js';
import {executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

const STATUS_TEXT_KEY_MAP: {[key in UpdateRoFirmwareStatus]: string} = {
  [UpdateRoFirmwareStatus.kUnknown]: '',
  [UpdateRoFirmwareStatus.kWaitUsb]: 'firmwareUpdateWaitForUsbText',
  [UpdateRoFirmwareStatus.kFileNotFound]: 'firmwareUpdateFileNotFoundText',
  [UpdateRoFirmwareStatus.kDownloading]: '',
  [UpdateRoFirmwareStatus.kUpdating]: 'firmwareUpdatingText',
  [UpdateRoFirmwareStatus.kRebooting]: 'firmwareUpdateRebootText',
  [UpdateRoFirmwareStatus.kComplete]: 'firmwareUpdateCompleteText',
};

const STATUS_IMG_MAP: {[key in UpdateRoFirmwareStatus]: string} = {
  [UpdateRoFirmwareStatus.kUnknown]: '',
  [UpdateRoFirmwareStatus.kWaitUsb]: 'insert_usb',
  [UpdateRoFirmwareStatus.kFileNotFound]: 'error',
  [UpdateRoFirmwareStatus.kDownloading]: '',
  [UpdateRoFirmwareStatus.kUpdating]: '',
  [UpdateRoFirmwareStatus.kRebooting]: 'downloading',
  [UpdateRoFirmwareStatus.kComplete]: 'downloading',
};

const STATUS_ALT_MAP: {[key in UpdateRoFirmwareStatus]: string} = {
  [UpdateRoFirmwareStatus.kUnknown]: '',
  [UpdateRoFirmwareStatus.kWaitUsb]: 'insertUsbAltText',
  [UpdateRoFirmwareStatus.kFileNotFound]: 'errorAltText',
  [UpdateRoFirmwareStatus.kDownloading]: '',
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

const UpdateRoFirmwarePageBase = I18nMixin(PolymerElement);

export class UpdateRoFirmwarePage extends UpdateRoFirmwarePageBase {
  static get is() {
    return 'reimaging-firmware-update-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      status: {
        type: Object,
        value: null,
      },

      statusString: {
        type: String,
      },

      shouldShowSpinner: {
        type: Boolean,
        value: false,
      },

      shouldShowWarning: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      imgSrc: {
        type: String,
        value: '',
      },

      imgAlt: {
        type: String,
        value: '',
      },
    };
  }

  protected status: UpdateRoFirmwareStatus|null;
  protected statusString: string;
  protected shouldShowSpinner: boolean;
  protected shouldShowWarning: boolean;
  protected imgSrc: string;
  protected imgAlt: string;
  private shimlessRmaService: ShimlessRmaServiceInterface =
      getShimlessRmaService();
  updateRoFirmwareObserverReceiver: UpdateRoFirmwareObserverReceiver;
  externalDiskStateReceiver: ExternalDiskStateObserverReceiver;

  constructor() {
    super();
    this.updateRoFirmwareObserverReceiver =
        new UpdateRoFirmwareObserverReceiver(this);

    this.shimlessRmaService.observeRoFirmwareUpdateProgress(
        this.updateRoFirmwareObserverReceiver.$.bindNewPipeAndPassRemote());

    this.externalDiskStateReceiver =
        new ExternalDiskStateObserverReceiver(this);

    this.shimlessRmaService.observeExternalDiskState(
        this.externalDiskStateReceiver.$.bindNewPipeAndPassRemote());
  }

  override ready() {
    super.ready();
    focusPageTitle(this);
  }

  static get observers() {
    return ['onStatusChanged(status)'];
  }

  /**
   * Implements UpdateRoFirmwareObserver.OnUpdateRoFirmwareStatusChanged()
   */
  onUpdateRoFirmwareStatusChanged(status: UpdateRoFirmwareStatus): void {
    this.status = status;
    this.shouldShowSpinner = this.status === UpdateRoFirmwareStatus.kUpdating;
    this.shouldShowWarning =
        this.status === UpdateRoFirmwareStatus.kFileNotFound;
  }

  /**
   * Implements ExternalDiskStateObserver.onExternalDiskStateChanged()
   */
  onExternalDiskStateChanged(detected: boolean): void {
    if (!detected && this.status === UpdateRoFirmwareStatus.kComplete) {
      executeThenTransitionState(
          this, () => this.shimlessRmaService.roFirmwareUpdateComplete());
    }
  }

  /**
   * Groups state changes related to the |status| updating.
   */
  protected onStatusChanged(): void {
    this.setStatusString();
    this.setImgSrcAndAlt();
  }

  protected setStatusString(): void {
    this.statusString =
        !this.status ? '' : this.i18n(STATUS_TEXT_KEY_MAP[this.status]);
  }

  protected setImgSrcAndAlt(): void {
    this.imgSrc = `illustrations/${
    !this.status ? 'downloading' : STATUS_IMG_MAP[this.status]}.svg`;
    this.imgAlt = this.i18n(
        !this.status ? 'downloadingAltText' : STATUS_ALT_MAP[this.status]);
  }

  protected getTitleText(): string {
    return this.i18n(
        this.status === UpdateRoFirmwareStatus.kComplete ?
            'firmwareUpdateInstallCompleteTitleText' :
            'firmwareUpdateInstallImageTitleText');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [UpdateRoFirmwarePage.is]: UpdateRoFirmwarePage;
  }
}

customElements.define(UpdateRoFirmwarePage.is, UpdateRoFirmwarePage);
