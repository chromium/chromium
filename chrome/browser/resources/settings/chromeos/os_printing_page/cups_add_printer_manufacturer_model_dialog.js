// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'add-printer-manufacturer-model-dialog' is a dialog in which the user can
 *   manually select the manufacture and model of the new printer.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './cups_add_printer_dialog.js';
import './cups_printer_dialog_error.js';
import './cups_printer_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {getBaseName, getErrorText, isPPDInfoValid} from './cups_printer_dialog_util.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, ManufacturersInfo, ModelsInfo, PrinterSetupResult} from './cups_printers_browser_proxy.js';

/** @polymer */
class AddPrinterManufacturerModelDialogElement extends PolymerElement {
  static get is() {
    return 'add-printer-manufacturer-model-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!CupsPrinterInfo} */
      activePrinter: {
        type: Object,
        notify: true,
      },

      /** @type {?Array<string>} */
      manufacturerList: Array,

      /** @type {?Array<string>} */
      modelList: Array,

      /**
       * Whether the user selected PPD file is valid.
       * @private
       */
      invalidPPD_: {
        type: Boolean,
        value: false,
      },

      /**
       * The base name of a newly selected PPD file.
       * @private
       */
      newUserPPD_: String,

      /**
       * The URL to a printer's EULA.
       * @private
       */
      eulaUrl_: {
        type: String,
        value: '',
      },

      /** @private */
      addPrinterInProgress_: {
        type: Boolean,
        value: false,
      },

      /**
       * The error text to be displayed on the dialog.
       * @private
       */
      errorText_: {
        type: String,
        value: '',
      },

      /**
       * Indicates whether the value in the Manufacturer dropdown is a valid
       * printer manufacturer.
       * @private
       */
      isManufacturerInvalid_: Boolean,

      /**
       * Indicates whether the value in the Model dropdown is a valid printer
       * model.
       * @private
       */
      isModelInvalid_: Boolean,
    };
  }

  static get observers() {
    return [
      'selectedManufacturerChanged_(activePrinter.ppdManufacturer)',
      'selectedModelChanged_(activePrinter.ppdModel)',

    ];
  }

  constructor() {
    super();

    /** @private {!CupsPrintersBrowserProxy} */
    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getCupsPrinterManufacturersList().then(
        this.manufacturerListChanged_.bind(this));
  }

  close() {
    this.shadowRoot.querySelector('add-printer-dialog').close();
  }

  /**
   * Handler for addCupsPrinter success.
   * @param {!PrinterSetupResult} result
   * @private
   */
  onPrinterAddedSucceeded_(result) {
    const showCupsPrinterToastEvent =
        new CustomEvent('show-cups-printer-toast', {
          bubbles: true,
          composed: true,
          detail:
              {resultCode: result, printerName: this.activePrinter.printerName},
        });
    this.dispatchEvent(showCupsPrinterToastEvent);
    this.close();
  }

  /**
   * Handler for addCupsPrinter failure.
   * @param {*} result
   * @private
   */
  onPrinterAddedFailed_(result) {
    this.addPrinterInProgress_ = false;
    this.errorText_ = getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  }

  /**
   * If the printer is a nearby printer, return make + model with the subtext.
   * Otherwise, return printer name.
   * @return {string} The additional information subtext of the manufacturer and
   * model dialog.
   * @private
   */
  getManufacturerAndModelSubtext_() {
    if (this.activePrinter.printerMakeAndModel) {
      return loadTimeData.getStringF(
          'manufacturerAndModelAdditionalInformation',
          this.activePrinter.printerMakeAndModel);
    }
    return loadTimeData.getStringF(
        'manufacturerAndModelAdditionalInformation',
        this.activePrinter.printerName);
  }

  /**
   * @param {string} manufacturer The manufacturer for which we are retrieving
   *     models.
   * @private
   */
  selectedManufacturerChanged_(manufacturer) {
    // Reset model if manufacturer is changed.
    this.set('activePrinter.ppdModel', '');
    this.modelList = [];
    if (manufacturer && manufacturer.length !== 0) {
      this.browserProxy_.getCupsPrinterModelsList(manufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  }

  /**
   * Attempts to get the EULA Url if the selected printer has one.
   * @private
   */
  selectedModelChanged_() {
    this.errorText_ = '';
    if (!this.activePrinter.ppdManufacturer || !this.activePrinter.ppdModel) {
      // Do not check for an EULA unless both |ppdManufacturer| and |ppdModel|
      // are set. Set |eulaUrl_| to be empty in this case.
      this.onGetEulaUrlCompleted_('' /* eulaUrl */);
      return;
    }

    this.browserProxy_
        .getEulaUrl(
            this.activePrinter.ppdManufacturer, this.activePrinter.ppdModel)
        .then(this.onGetEulaUrlCompleted_.bind(this));
  }

  /**
   * @param {string} eulaUrl The URL for the printer's EULA.
   * @private
   */
  onGetEulaUrlCompleted_(eulaUrl) {
    this.eulaUrl_ = eulaUrl;
  }

  /**
   * @param {!ManufacturersInfo} manufacturersInfo
   * @private
   */
  manufacturerListChanged_(manufacturersInfo) {
    if (!manufacturersInfo.success) {
      return;
    }
    this.manufacturerList = manufacturersInfo.manufacturers;
    if (this.activePrinter.ppdManufacturer.length !== 0) {
      this.browserProxy_
          .getCupsPrinterModelsList(this.activePrinter.ppdManufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  }

  /**
   * @param {!ModelsInfo} modelsInfo
   * @private
   */
  modelListChanged_(modelsInfo) {
    if (modelsInfo.success) {
      this.modelList = modelsInfo.models;
    }
  }

  /** @private */
  onBrowseFile_() {
    this.browserProxy_.getCupsPrinterPPDPath().then(
        this.printerPPDPathChanged_.bind(this));
  }

  /**
   * @param {string} path The full path to the selected PPD file
   * @private
   */
  printerPPDPathChanged_(path) {
    this.set('activePrinter.printerPPDPath', path);
    this.invalidPPD_ = !path;
    this.newUserPPD_ = getBaseName(path);
  }

  /** @private */
  onCancelTap_() {
    this.close();
    this.browserProxy_.cancelPrinterSetUp(this.activePrinter);
  }

  /** @private */
  addPrinter_() {
    this.addPrinterInProgress_ = true;
    this.browserProxy_.addCupsPrinter(this.activePrinter)
        .then(
            this.onPrinterAddedSucceeded_.bind(this),
            this.onPrinterAddedFailed_.bind(this));
  }

  /**
   * @param {string} ppdManufacturer
   * @param {string} ppdModel
   * @param {string} printerPPDPath
   * @return {boolean} Whether we have enough information to set up the printer
   * @private
   */
  canAddPrinter_(
      ppdManufacturer, ppdModel, printerPPDPath, addPrinterInProgress,
      isManufacturerInvalid, isModelInvalid) {
    return !addPrinterInProgress &&
        isPPDInfoValid(ppdManufacturer, ppdModel, printerPPDPath) &&
        !isManufacturerInvalid && !isModelInvalid;
  }
}

customElements.define(
    AddPrinterManufacturerModelDialogElement.is,
    AddPrinterManufacturerModelDialogElement);
