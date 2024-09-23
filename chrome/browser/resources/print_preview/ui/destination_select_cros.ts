// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/util.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './destination_dropdown_cros.js';
import './destination_select_style.css.js';
import './icons.html.js';
import './print_preview_shared.css.js';
import './throbber.css.js';
import '../strings.m.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import {DestinationOrigin, GooglePromotedDestinationId, PDF_DESTINATION_KEY} from '../data/destination.js';
import {ERROR_STRING_KEY_MAP, getPrinterStatusIcon, getStatusTextColorClass, PrinterStatusReason} from '../data/printer_status_cros.js';
import type {Error, State} from '../data/state.js';

import type {PrintPreviewDestinationDropdownCrosElement} from './destination_dropdown_cros.js';
import {getTemplate} from './destination_select_cros.html.js';
import {shouldShowCrosPrinterSetupError} from './preview_area.js';
import {SelectMixin} from './select_mixin.js';
import type {PrintPreviewSettingsSectionElement} from './settings_section.js';

export interface PrintPreviewDestinationSelectCrosElement {
  $: {
    destinationEulaWrapper: PrintPreviewSettingsSectionElement,
    dropdown: PrintPreviewDestinationDropdownCrosElement,
  };
}

const PrintPreviewDestinationSelectCrosElementBase =
    I18nMixin(SelectMixin(PolymerElement));

export class PrintPreviewDestinationSelectCrosElement extends
    PrintPreviewDestinationSelectCrosElementBase {
  static get is() {
    return 'print-preview-destination-select-cros';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activeUser: String,

      dark: Boolean,

      destination: Object,

      disabled: Boolean,

      driveDestinationKey: String,

      loaded: Boolean,

      noDestinations: Boolean,

      pdfPrinterDisabled: Boolean,

      recentDestinationList: {
        type: Array,
        observer: 'onRecentDestinationListChanged_',
      },

      pdfDestinationKey_: {
        type: String,
        value: PDF_DESTINATION_KEY,
      },

      statusText_: {
        type: String,
        computed: 'computeStatusText_(destination, ' +
            'destination.printerStatusReason, state, error)',
      },

      destinationIcon_: {
        type: String,
        computed: 'computeDestinationIcon_(' +
            'selectedValue, destination, destination.printerStatusReason,' +
            'isDarkModeActive_, state, error)',
      },

      isCurrentDestinationCrosLocal_: {
        type: Boolean,
        computed: 'computeIsCurrentDestinationCrosLocal_(destination)',
        reflectToAttribute: true,
      },

      // Holds status of iron-media-query (prefers-color-scheme: dark).
      isDarkModeActive_: Boolean,

      state: Number,

      error: Number,
    };
  }

  destination: Destination;
  disabled: boolean;
  loaded: boolean;
  pdfPrinterDisabled: boolean;
  recentDestinationList: Destination[];
  state: State;
  error: Error;
  private pdfDestinationKey_: string;
  private statusText_: TrustedHTML;
  private destinationIcon_: string;
  private isCurrentDestinationCrosLocal_: boolean;
  private isDarkModeActive_: boolean;

  override focus() {
    this.shadowRoot!.querySelector(
                        'print-preview-destination-dropdown-cros')!.focus();
  }

  /** Sets the select to the current value of |destination|. */
  updateDestination() {
    this.selectedValue = this.destination.key;
  }

  /**
   * Returns the iconset and icon for the selected printer. If printer details
   * have not yet been retrieved from the backend, attempts to return an
   * appropriate icon early based on the printer's sticky information.
   * @return The iconset and icon for the current selection.
   */
  private computeDestinationIcon_(): string {
    if (!this.selectedValue) {
      return '';
    }

    // If the destination matches the selected value, pull the icon from the
    // destination.
    if (this.destination && this.destination.key === this.selectedValue) {
      if (this.isCurrentDestinationCrosLocal_) {
        // Override the printer status icon if the printer setup info UI is
        // showing.
        if (shouldShowCrosPrinterSetupError(this.state, this.error)) {
          return getPrinterStatusIcon(
              PrinterStatusReason.PRINTER_UNREACHABLE,
              this.destination.isEnterprisePrinter, this.isDarkModeActive_);
        }

        return getPrinterStatusIcon(
            this.destination.printerStatusReason,
            this.destination.isEnterprisePrinter, this.isDarkModeActive_);
      }

      return this.destination.icon;
    }

    // Check for the Docs or Save as PDF ids first.
    const keyParams = this.selectedValue.split('/');
    if (keyParams[0] === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
      return 'print-preview:save-to-drive';
    }

    if (keyParams[0] === GooglePromotedDestinationId.SAVE_AS_PDF) {
      return 'cr:insert-drive-file';
    }

    // Otherwise, must be in the recent list.
    const recent = this.recentDestinationList.find(d => {
      return d.key === this.selectedValue;
    });
    if (recent && recent.icon) {
      return recent.icon;
    }

    // The key/recent destinations don't have information about what icon to
    // use, so just return the generic print icon for now. It will be updated
    // when the destination is set.
    return 'print-preview:print';
  }

  private hideDestinationAdditionalInfo_(): boolean {
    return this.statusText_ === window.trustedTypes!.emptyHTML;
  }

  private fireSelectedOptionChange_(value: string) {
    this.dispatchEvent(new CustomEvent(
        'selected-option-change',
        {bubbles: true, composed: true, detail: value}));
  }

  override onProcessSelectChange(value: string) {
    this.fireSelectedOptionChange_(value);
  }

  private onDropdownValueSelected_(e: CustomEvent<HTMLButtonElement>) {
    const selectedItem = e.detail;
    if (!selectedItem || selectedItem.value === this.destination.key) {
      return;
    }

    this.fireSelectedOptionChange_(selectedItem.value);
  }

  /**
   * Send a printer status request for any new destination in the dropdown.
   */
  private onRecentDestinationListChanged_() {
    for (const destination of this.recentDestinationList) {
      if (!destination || destination.origin !== DestinationOrigin.CROS) {
        continue;
      }

      destination.requestPrinterStatus().then(
          destinationKey => this.onPrinterStatusReceived_(destinationKey));
    }
  }

  /**
   * Check if the printer is currently in the dropdown then update its status
   *    icon if it's present.
   */
  private onPrinterStatusReceived_(destinationKey: string) {
    const indexFound = this.recentDestinationList.findIndex(destination => {
      return destination.key === destinationKey;
    });
    if (indexFound === -1) {
      return;
    }

    // Use notifyPath to trigger the matching printer located in the dropdown to
    // recalculate its status icon.
    this.notifyPath(`recentDestinationList.${indexFound}.printerStatusReason`);

    // If |destinationKey| matches the currently selected printer, use
    // notifyPath to trigger the destination to recalculate its status icon and
    // error status text.
    if (this.destination && this.destination.key === destinationKey) {
      this.notifyPath(`destination.printerStatusReason`);
    }
  }

  /**
   * @return An error status for the current destination. If no error
   *     status exists, an empty string.
   */
  private computeStatusText_(): TrustedHTML {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return window.trustedTypes!.emptyHTML;
    }

    // Non-local printers do not show an error status.
    if (this.destination.origin !== DestinationOrigin.CROS) {
      return window.trustedTypes!.emptyHTML;
    }

    // Override the printer status text if the printer setup info UI is showing.
    if (shouldShowCrosPrinterSetupError(this.state, this.error)) {
      return this.getErrorString_(PrinterStatusReason.PRINTER_UNREACHABLE);
    }

    const printerStatusReason = this.destination.printerStatusReason;
    if (printerStatusReason === null ||
        printerStatusReason === PrinterStatusReason.NO_ERROR ||
        printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
      return window.trustedTypes!.emptyHTML;
    }

    return this.getErrorString_(printerStatusReason);
  }

  private getErrorString_(printerStatusReason: PrinterStatusReason):
      TrustedHTML {
    const errorStringKey = ERROR_STRING_KEY_MAP.get(printerStatusReason);
    return errorStringKey ? this.i18nAdvanced(errorStringKey) :
                            window.trustedTypes!.emptyHTML;
  }

  /**
   * True when the currently selected destination is a CrOS local printer.
   */
  private computeIsCurrentDestinationCrosLocal_(): boolean {
    return this.destination &&
        this.destination.origin === DestinationOrigin.CROS;
  }

  private computeStatusClass_(): string {
    const statusClass = 'destination-status';
    if (!this.destination) {
      return statusClass;
    }

    const printerStatusReason = this.destination.printerStatusReason;
    if (printerStatusReason === null ||
        printerStatusReason === PrinterStatusReason.NO_ERROR ||
        printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
      return statusClass;
    }

    return `${statusClass} ${getStatusTextColorClass(printerStatusReason)}`;
  }

  /**
   * Return the options currently visible to the user for testing purposes.
   */
  getVisibleItemsForTest(): NodeListOf<HTMLButtonElement> {
    return this.shadowRoot!.querySelector('#dropdown')!.shadowRoot!
        .querySelectorAll<HTMLButtonElement>('.list-item:not([hidden])');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-select-cros':
        PrintPreviewDestinationSelectCrosElement;
  }
}

customElements.define(
    PrintPreviewDestinationSelectCrosElement.is,
    PrintPreviewDestinationSelectCrosElement);
