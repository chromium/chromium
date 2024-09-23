// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying cellular EID and QR code
 */
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {EuiccProperties, EuiccRemote, QRCode} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_device_info_dialog.html.js';

// The size of each tile/module in pixels.
const QR_CODE_TILE_SIZE = 5;

// The quiet zone offset in tiles/modules surrounding a QR code.
const QUIET_ZONE_OFFSET = 4;

// Styling for filled tiles in the QR code.
const QR_CODE_FILL_STYLE = '#000000';

export interface NetworkDeviceInfoDialogElement {
  $: {
    done: CrButtonElement,
    deviceInfoDialog: CrDialogElement,
  };
}

export class NetworkDeviceInfoDialogElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'network-device-info-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The euicc object whose EID and QRCode should be shown in the dialog.
       */
      euicc: Object,

      /**
       * Device state properties for the network subpage.
       */
      deviceState: Object,

      canvasSize_: Number,

      eid_: String,
    };
  }

  euicc: EuiccRemote|undefined;
  deviceState: OncMojo.DeviceStateProperties|undefined;
  private canvasSize_: number;
  private eid_: string|undefined;
  private canvasContext_: CanvasRenderingContext2D|null;

    override ready(): void {
      super.ready();

      if (this.euicc) {
        this.fetchEid_(this.euicc);
        return;
      }

      requestAnimationFrame(() => {
        this.$.done.focus();
      });
    }

    private onDonePressed_(): void {
      this.$.deviceInfoDialog.close();
    }

    private async fetchEid_(euicc: EuiccRemote): Promise<void> {
      const [qrCodeResponse, euiccPropertiesResponse] = await Promise.all([
        euicc.getEidQRCode(),
        euicc.getProperties(),
      ]);
      this.updateEid_(euiccPropertiesResponse?.properties);
      this.renderQrCode_(qrCodeResponse?.qrCode);
    }

    private renderQrCode_(qrCode: QRCode|null): void {
      if (!qrCode) {
        return;
      }
      this.canvasSize_ = qrCode.size * QR_CODE_TILE_SIZE +
          2 * QUIET_ZONE_OFFSET * QR_CODE_TILE_SIZE;
      flush();
      const context = this.getCanvasContext_();
      if (!context) {
        return;
      }
      context.clearRect(0, 0, this.canvasSize_, this.canvasSize_);
      context.fillStyle = QR_CODE_FILL_STYLE;
      let index = 0;
      for (let x = QUIET_ZONE_OFFSET; x < qrCode.size + QUIET_ZONE_OFFSET;
           x++) {
        for (let y = QUIET_ZONE_OFFSET; y < qrCode.size + QUIET_ZONE_OFFSET;
             y++) {
          if (qrCode.data[index]) {
            context.fillRect(
                x * QR_CODE_TILE_SIZE, y * QR_CODE_TILE_SIZE, QR_CODE_TILE_SIZE,
                QR_CODE_TILE_SIZE);
          }
          index++;
        }
      }
    }

    private updateEid_(euiccProperties: EuiccProperties|null): void {
      if (!euiccProperties) {
        return;
      }
      this.eid_ = euiccProperties.eid;
    }

    private getCanvasContext_(): CanvasRenderingContext2D|null {
      if (this.canvasContext_) {
        return this.canvasContext_;
      }
      const canvas =
          this.shadowRoot!.querySelector<HTMLCanvasElement>('#qrCodeCanvas');
      return canvas!.getContext('2d');
    }

    private shouldShowEidAndQrCode_(): boolean {
      return !!this.eid_;
    }

    private shouldShowImei_(): boolean {
      return !!this.deviceState?.imei;
    }

    private shouldShowSerial_(): boolean {
      return !!this.deviceState?.serial;
    }

    setCanvasContextForTest(canvasContext: CanvasRenderingContext2D): void {
      this.canvasContext_ = canvasContext;
    }

    private getA11yLabel_(): string {
      if (this.eid_ && this.deviceState?.imei && this.deviceState?.serial) {
        return this.i18n(
            'deviceInfoPopupA11yEidImeiAndSerial', this.eid_,
            this.deviceState.imei, this.deviceState.serial);
      }

      if (this.eid_) {
        if (this.deviceState?.imei) {
          return this.i18n(
              'deviceInfoPopupA11yEidAndImei', this.eid_,
              this.deviceState.imei);
        }
        if (this.deviceState?.serial) {
          return this.i18n(
              'deviceInfoPopupA11yEidAndSerial', this.eid_,
              this.deviceState.serial);
        }
        return this.i18n('deviceInfoPopupA11yEid', this.eid_);
      }

      if (this.deviceState?.imei) {
        if (this.deviceState?.serial) {
          return this.i18n(
              'deviceInfoPopupA11yImeiAndSerial', this.deviceState.imei,
              this.deviceState.serial);
        }
        return this.i18n('deviceInfoPopupA11yImei', this.deviceState.imei);
      }

      if (this.deviceState?.serial) {
        return this.i18n('deviceInfoPopupA11ySerial', this.deviceState.serial);
      }

      return '';
    }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkDeviceInfoDialogElement.is]: NetworkDeviceInfoDialogElement;
  }
}

customElements.define(
    NetworkDeviceInfoDialogElement.is, NetworkDeviceInfoDialogElement);
