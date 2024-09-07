// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './print_preview_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsContext, PrintPreviewLaunchSourceBucket} from '../metrics.js';
import type {NativeLayer} from '../native_layer.js';
import {NativeLayerImpl} from '../native_layer.js';
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

// Minimum values used to hide the illustration when the preview area is reduced
// to a small size.
const MIN_SHOW_ILLUSTRATION_HEIGHT = 400;
const MIN_SHOW_ILLUSTRATION_WIDTH = 250;

export enum PrinterSetupInfoInitiator {
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

      initiator: Number,

      showManagePrintersButton: Boolean,

      showIllustration: Boolean,
    };
  }

  messageType: PrinterSetupInfoMessageType;
  private initiator: PrinterSetupInfoInitiator;
  private nativeLayer: NativeLayer;
  private metricsContext: MetricsContext;
  private showManagePrintersButton: boolean = false;
  private showIllustration: boolean = true;
  private resizeObserver: ResizeObserver;

  override connectedCallback() {
    super.connectedCallback();
    this.nativeLayer = NativeLayerImpl.getInstance();
    this.metricsContext =
        MetricsContext.getLaunchPrinterSettingsMetricsContextCros();
    NativeLayerCrosImpl.getInstance().getShowManagePrinters().then(
        (show: boolean) => {
          this.showManagePrintersButton = show;
        });

    // If this is Print Preview, observe the window resizing to know when to
    // hide the illustration.
    if (this.initiator === PrinterSetupInfoInitiator.PREVIEW_AREA) {
      this.startResizeObserver();
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    if (this.initiator === PrinterSetupInfoInitiator.PREVIEW_AREA) {
      this.resizeObserver.disconnect();
    }
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
    switch (this.initiator) {
      case PrinterSetupInfoInitiator.PREVIEW_AREA:
        this.metricsContext.record(
            PrintPreviewLaunchSourceBucket.PREVIEW_AREA_CONNECTION_ERROR);
        break;
      case PrinterSetupInfoInitiator.DESTINATION_DIALOG_CROS:
        // `<print-preview-printer-setup-info-cros>` is only displayed when
        // there are no printers.
        this.metricsContext.record(
            PrintPreviewLaunchSourceBucket.DESTINATION_DIALOG_CROS_NO_PRINTERS);
        break;
      default:
        assertNotReached();
    }
  }

  private setShowIllustration(): void {
    assert(this.initiator === PrinterSetupInfoInitiator.PREVIEW_AREA);

    // Only show the illustration if the parent element's width and height are
    // wide enough.
    const parentDiv = this.getPreviewAreaParentDiv();
    this.showIllustration =
        parentDiv.offsetHeight >= MIN_SHOW_ILLUSTRATION_HEIGHT &&
        parentDiv.offsetWidth >= MIN_SHOW_ILLUSTRATION_WIDTH;
  }

  private getPreviewAreaParentDiv(): HTMLElement {
    assert(this.initiator === PrinterSetupInfoInitiator.PREVIEW_AREA);

    const parentShadowRoot = this.shadowRoot!.host.getRootNode() as ShadowRoot;
    assert(parentShadowRoot);
    const previewContainer =
        parentShadowRoot!.querySelector<HTMLElement>('.preview-area-message');
    assert(previewContainer);
    return previewContainer;
  }

  private startResizeObserver(): void {
    // Set timeout to 0 to delay the callback action to the next event cycle.
    this.resizeObserver = new ResizeObserver(
        () => setTimeout(() => this.setShowIllustration(), 0));
    this.resizeObserver.observe(this.getPreviewAreaParentDiv());
  }

  setInitiatorForTesting(
      initiator: PrinterSetupInfoInitiator,
      startResizeObserver: boolean): void {
    this.initiator = initiator;
    if (this.initiator === PrinterSetupInfoInitiator.PREVIEW_AREA) {
      if (startResizeObserver) {
        this.startResizeObserver();
      } else {
        // Most tests don't need an resize observer with an active callback.
        this.resizeObserver = new ResizeObserver(() => {});
      }
    }
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
