// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'add-printer-manufacturer-model-dialog' is a dialog in which the user can
 *   manually select the manufacture and model of the new printer.
 */
Polymer({
  is: 'add-printer-manufacturer-model-dialog',

  properties: {
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
  },

  observers: [
    'selectedManufacturerChanged_(activePrinter.ppdManufacturer)',
    'selectedModelChanged_(activePrinter.ppdModel)',
  ],

  /** @override */
  attached() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterManufacturersList()
        .then(this.manufacturerListChanged_.bind(this));
  },

  close() {
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter success.
   * @param {!PrinterSetupResult} result
   * @private
   */
  onPrinterAddedSucceeded_(result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: this.activePrinter.printerName});
    this.close();
  },

  /**
   * Handler for addCupsPrinter failure.
   * @param {*} result
   * @private
   */
  onPrinterAddedFailed_(result) {
    this.addPrinterInProgress_ = false;
    this.errorText_ = settings.printing.getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  },

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
  },

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
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(manufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

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

    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getEulaUrl(
            this.activePrinter.ppdManufacturer, this.activePrinter.ppdModel)
        .then(this.onGetEulaUrlCompleted_.bind(this));
  },

  /**
   * @param {string} eulaUrl The URL for the printer's EULA.
   * @private
   */
  onGetEulaUrlCompleted_(eulaUrl) {
    this.eulaUrl_ = eulaUrl;
  },

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
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(this.activePrinter.ppdManufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /**
   * @param {!ModelsInfo} modelsInfo
   * @private
   */
  modelListChanged_(modelsInfo) {
    if (modelsInfo.success) {
      this.modelList = modelsInfo.models;
    }
  },

  /** @private */
  onBrowseFile_() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterPPDPath()
        .then(this.printerPPDPathChanged_.bind(this));
  },

  /**
   * @param {string} path The full path to the selected PPD file
   * @private
   */
  printerPPDPathChanged_(path) {
    this.set('activePrinter.printerPPDPath', path);
    this.invalidPPD_ = !path;
    this.newUserPPD_ = settings.printing.getBaseName(path);
  },

  /** @private */
  onCancelTap_() {
    this.close();
    settings.CupsPrintersBrowserProxyImpl.getInstance().cancelPrinterSetUp(
        this.activePrinter);
  },

  /** @private */
  addPrinter_() {
    this.addPrinterInProgress_ = true;
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .addCupsPrinter(this.activePrinter)
        .then(
            this.onPrinterAddedSucceeded_.bind(this),
            this.onPrinterAddedFailed_.bind(this));
  },

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
        settings.printing.isPPDInfoValid(
            ppdManufacturer, ppdModel, printerPPDPath) &&
        !isManufacturerInvalid && !isModelInvalid;
  },
});
