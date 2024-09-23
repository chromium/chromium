// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './destination_list_item_style.css.js';
import './icons.html.js';
import '../strings.m.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {removeHighlights} from 'chrome://resources/js/search_highlight_utils.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import {DestinationOrigin} from '../data/destination.js';
import {ERROR_STRING_KEY_MAP, getPrinterStatusIcon, getStatusTextColorClass, PrinterStatusReason} from '../data/printer_status_cros.js';

import {getTemplate} from './destination_list_item_cros.html.js';
import {updateHighlights} from './highlight_utils.js';

enum DestinationConfigStatus {
  IDLE = 0,
  IN_PROGRESS = 1,
  FAILED = 2,
}

const PrintPreviewDestinationListItemElementBase = I18nMixin(PolymerElement);

export class PrintPreviewDestinationListItemElement extends
    PrintPreviewDestinationListItemElementBase {
  static get is() {
    return 'print-preview-destination-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      destination: Object,
      searchQuery: Object,
      searchHint_: String,

      destinationIcon_: {
        type: String,
        computed: 'computeDestinationIcon_(destination, ' +
            'destination.printerStatusReason)',
      },

      statusText_: {
        type: String,
        computed:
            'computeStatusText_(destination, destination.printerStatusReason,' +
            'configurationStatus_)',
      },

      // Holds status of iron-media-query (prefers-color-scheme: dark).
      isDarkModeActive_: Boolean,

      isDestinationCrosLocal_: {
        type: Boolean,
        computed: 'computeIsDestinationCrosLocal_(destination)',
        reflectToAttribute: true,
      },

      configurationStatus_: {
        type: Number,
        value: DestinationConfigStatus.IDLE,
      },

      /**
       * Mirroring the enum so that it can be used from HTML bindings.
       */
      statusEnum_: {
        type: Object,
        value: DestinationConfigStatus,
      },
    };
  }

  static get observers() {
    return [
      'onDestinationPropertiesChange_(' +
          'destination.displayName, destination.isExtension)',
      'updateHighlightsAndHint_(destination, searchQuery)',
      'requestPrinterStatus_(destination.key)',
    ];
  }

  destination: Destination;
  searchQuery: RegExp|null;
  private destinationIcon_: string;
  private searchHint_: string;
  private statusText_: string;
  private isDarkModeActive_: boolean;
  private isDestinationCrosLocal_: boolean;
  private configurationStatus_: DestinationConfigStatus;

  private highlights_: HTMLElement[] = [];

  private onDestinationPropertiesChange_() {
    this.title = this.destination.displayName;
    if (this.destination.isExtension) {
      const icon =
          this.shadowRoot!.querySelector<HTMLElement>('.extension-icon');
      assert(icon);
      icon.style.backgroundImage = 'image-set(' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/24/1) 1x,' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/48/1) 2x)';
    }
  }

  private updateHighlightsAndHint_() {
    this.updateSearchHint_();
    removeHighlights(this.highlights_);
    this.highlights_ = updateHighlights(this, this.searchQuery, new Map());
  }

  private updateSearchHint_() {
    const matches = !this.searchQuery ?
        [] :
        this.destination.extraPropertiesToMatch.filter(
            p => p.match(this.searchQuery!));
    this.searchHint_ = matches.length === 0 ?
        (this.destination.extraPropertiesToMatch.find(p => !!p) || '') :
        matches.join(' ');
  }

  private getExtensionPrinterTooltip_(): string {
    if (!this.destination.isExtension) {
      return '';
    }
    return loadTimeData.getStringF(
        'extensionDestinationIconTooltip', this.destination.extensionName);
  }

  /**
   * Called if the printer configuration request is accepted. Show the waiting
   * message to the user as the configuration might take longer than expected.
   */
  onConfigureRequestAccepted() {
    // It must be a Chrome OS CUPS printer which hasn't been set up before.
    assert(
        this.destination.origin === DestinationOrigin.CROS &&
        !this.destination.capabilities);
    this.configurationStatus_ = DestinationConfigStatus.IN_PROGRESS;
  }

  /**
   * Called when the printer configuration request completes.
   * @param success Whether configuration was successful.
   */
  onConfigureComplete(success: boolean) {
    this.configurationStatus_ =
        success ? DestinationConfigStatus.IDLE : DestinationConfigStatus.FAILED;
  }

  /**
   * @return Whether the current configuration status is |status|.
   */
  private checkConfigurationStatus_(status: DestinationConfigStatus): boolean {
    return this.configurationStatus_ === status;
  }

  /**
   * @return If the destination is a local CrOS printer, this returns
   *    the error text associated with the printer status.
   */
  private computeStatusText_(): string {
    if (!this.destination ||
        this.destination.origin !== DestinationOrigin.CROS) {
      return '';
    }

    // Don't show status text when destination is configuring.
    if (this.configurationStatus_ !== DestinationConfigStatus.IDLE) {
      return '';
    }

    const printerStatusReason = this.destination.printerStatusReason;
    if (printerStatusReason === null ||
        printerStatusReason === PrinterStatusReason.NO_ERROR ||
        printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
      return '';
    }

    const errorStringKey = ERROR_STRING_KEY_MAP.get(printerStatusReason);
    return errorStringKey ? this.i18n(errorStringKey) : '';
  }

  private computeDestinationIcon_(): string {
    if (!this.destination) {
      return '';
    }

    if (this.destination.origin === DestinationOrigin.CROS) {
      return getPrinterStatusIcon(
          this.destination.printerStatusReason,
          this.destination.isEnterprisePrinter, this.isDarkModeActive_);
    }

    return this.destination.icon;
  }

  private computeStatusClass_(): string {
    const statusClass = 'connection-status';
    if (!this.destination || this.destination.printerStatusReason === null) {
      return statusClass;
    }

    return `${statusClass} ${
        getStatusTextColorClass(this.destination.printerStatusReason)}`;
  }

  /**
   * True when the destination is a CrOS local printer.
   */
  private computeIsDestinationCrosLocal_(): boolean {
    return this.destination &&
        this.destination.origin === DestinationOrigin.CROS;
  }

  private requestPrinterStatus_() {
    // Requesting printer status only allowed for local CrOS printers.
    if (this.destination.origin !== DestinationOrigin.CROS) {
      return;
    }

    this.destination.requestPrinterStatus().then(
        destinationKey => this.onPrinterStatusReceived_(destinationKey));
  }

  private onPrinterStatusReceived_(destinationKey: string) {
    if (this.destination.key === destinationKey) {
      // Notify printerStatusReason to trigger icon and status text update.
      this.notifyPath(`destination.printerStatusReason`);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-list-item':
        PrintPreviewDestinationListItemElement;
  }
}

customElements.define(
    PrintPreviewDestinationListItemElement.is,
    PrintPreviewDestinationListItemElement);
