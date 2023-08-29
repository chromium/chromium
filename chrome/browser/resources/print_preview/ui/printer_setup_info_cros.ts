// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './print_preview_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsContext, PrintPreviewLaunchSourceBucket} from '../metrics.js';
import {NativeLayer, NativeLayerImpl} from '../native_layer.js';
import {NativeLayerCrosImpl} from '../native_layer_cros.js';

import {getTemplate} from './printer_setup_info_cros.html.js';

/**
 * @fileoverview PrintPreviewPrinterSetupInfoCrosElement
 * This element provides contextual instructions to help users navigate
 * to printer settings based on the state of printers available in
 * print-preview. Element will use NativeLayer to open the correct printer
 * settings interface.
 */

const PrintPreviewPrinterSetupInfoCrosElementBase = I18nMixin(PolymerElement);

export enum PrinterSetupInfoMetricsSource {
  PREVIEW_AREA,
  DESTINATION_DIALOG_CROS,
}

export enum PrinterSetupInfoMessageType {
  NO_PRINTERS,
  PRINTER_OFFLINE,
}

interface PrinterSetupInfoMessageData {
  detailKey: string;
  headingKey: string;
}

const MESSAGE_TYPE_LOCALIZED_STRINGS_MAP =
    new Map<PrinterSetupInfoMessageType, PrinterSetupInfoMessageData>([
      [
        PrinterSetupInfoMessageType.NO_PRINTERS,
        {
          headingKey: 'printerSetupInfoMessageHeadingNoPrintersText',
          detailKey: 'printerSetupInfoMessageDetailNoPrintersText',
        },
      ],
      [
        PrinterSetupInfoMessageType.PRINTER_OFFLINE,
        {
          headingKey: 'printerSetupInfoMessageHeadingPrinterOfflineText',
          detailKey: 'printerSetupInfoMessageDetailPrinterOfflineText',
        },
      ],
    ]);

export class PrintPreviewPrinterSetupInfoCrosElement extends
    PrintPreviewPrinterSetupInfoCrosElementBase {
  static get is() {
    return 'print-preview-printer-setup-info-cros' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      messageType: {
        type: Number,
        value: PrinterSetupInfoMessageType.NO_PRINTERS,
      },

      metricsSource: Number,

      showManagePrintersButton: Boolean,
    };
  }

  messageType: PrinterSetupInfoMessageType;
  private metricsSource: PrinterSetupInfoMetricsSource;
  private nativeLayer: NativeLayer;
  private metricsContext: MetricsContext;
  private showManagePrintersButton: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.nativeLayer = NativeLayerImpl.getInstance();
    this.metricsContext =
        MetricsContext.getLaunchPrinterSettingsMetricsContextCros();
    NativeLayerCrosImpl.getInstance().getShowManagePrinters().then(
        (show: boolean) => {
          this.showManagePrintersButton = show;
        });
  }

  private getMessageDetail(): string {
    const messageData =
        MESSAGE_TYPE_LOCALIZED_STRINGS_MAP.get(this.messageType);
    assert(messageData);
    return this.i18n(messageData!.detailKey);
  }

  private getMessageHeading(): string {
    const messageData =
        MESSAGE_TYPE_LOCALIZED_STRINGS_MAP.get(this.messageType);
    assert(messageData);
    return this.i18n(messageData!.headingKey);
  }

  private onManagePrintersClicked(): void {
    this.nativeLayer.managePrinters();
    switch (this.metricsSource) {
      case PrinterSetupInfoMetricsSource.PREVIEW_AREA:
        this.metricsContext.record(
            PrintPreviewLaunchSourceBucket.PREVIEW_AREA_CONNECTION_ERROR);
        break;
      case PrinterSetupInfoMetricsSource.DESTINATION_DIALOG_CROS:
        // `<print-preview-printer-setup-info-cros>` is only displayed when
        // there are no printers.
        this.metricsContext.record(
            PrintPreviewLaunchSourceBucket.DESTINATION_DIALOG_CROS_NO_PRINTERS);
        break;
      default:
        assertNotReached();
    }
  }

  setMetricsSourceForTesting(source: PrinterSetupInfoMetricsSource): void {
    this.metricsSource = source;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PrintPreviewPrinterSetupInfoCrosElement.is]:
        PrintPreviewPrinterSetupInfoCrosElement;
  }
}

customElements.define(
    PrintPreviewPrinterSetupInfoCrosElement.is,
    PrintPreviewPrinterSetupInfoCrosElement);
