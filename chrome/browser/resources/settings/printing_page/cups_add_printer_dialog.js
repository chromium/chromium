// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-add-printer-dialog' includes multiple dialogs to
 * set up a new CUPS printer.
 * Subdialogs include:
 * - 'add-printer-discovery-dialog' is a dialog showing discovered printers on
 *   the network that are available for setup.
 * - 'add-printer-manually-dialog' is a dialog in which user can manually enter
 *   the information to set up a new printer.
 * - 'add-printer-configuring-dialog' is the configuring-in-progress dialog.
 * - 'add-printer-manufacturer-model-dialog' is a dialog in which the user can
 *   manually select the manufacture and model of the new printer.
 */

/**
 * Different dialogs in add printer flow.
 * @enum {string}
 */
const AddPrinterDialogs = {
  DISCOVERY: 'add-printer-discovery-dialog',
  MANUALLY: 'add-printer-manually-dialog',
  CONFIGURING: 'add-printer-configuring-dialog',
  MANUFACTURER: 'add-printer-manufacturer-model-dialog',
};

/**
 * The maximum height of the discovered printers list when the searching spinner
 * is not showing.
 * @type {number}
 */
const kPrinterListFullHeight = 350;

/**
 * Return a reset CupsPrinterInfo object.
 *  @return {!CupsPrinterInfo}
 */
function getEmptyPrinter_() {
  return {
    ppdManufacturer: '',
    ppdModel: '',
    printerAddress: '',
    printerDescription: '',
    printerId: '',
    printerManufacturer: '',
    printerModel: '',
    printerMakeAndModel: '',
    printerName: '',
    printerPPDPath: '',
    printerPpdReference: {
      userSuppliedPpdUrl: '',
      effectiveMakeAndModel: '',
      autoconf: false,
    },
    printerPpdReferenceResolved: false,
    printerProtocol: 'ipp',
    printerQueue: 'ipp/print',
    printerStatus: '',
  };
}

Polymer({
  is: 'add-printer-discovery-dialog',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {!Array<!CupsPrinterInfo>|undefined} */
    discoveredPrinters: {
      type: Array,
      value: () => [],
    },

    /** @type {!CupsPrinterInfo} */
    selectedPrinter: {
      type: Object,
      notify: true,
    },

    discovering_: {
      type: Boolean,
      value: true,
    },

    /**
     * TODO(jimmyxgong): Remove this feature flag conditional once feature
     * is launched.
     * @private
     */
    enableUpdatedUi: Boolean,
  },

  /** @override */
  ready: function() {
    if (this.enableUpdatedUi) {
      return;
    }

    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .startDiscoveringPrinters();
    this.addWebUIListener(
        'on-nearby-printers-changed', this.onNearbyPrintersChanged_.bind(this));
    this.addWebUIListener(
        'on-printer-discovery-done', this.onPrinterDiscoveryDone_.bind(this));
  },

  close: function() {
    this.$$('add-printer-dialog').close();
  },

  /**
   * @param {!Array<!CupsPrinterInfo>} automaticPrinters
   * @param {!Array<!CupsPrinterInfo>} discoveredPrinters
   * @private
   */
  onNearbyPrintersChanged_: function(automaticPrinters, discoveredPrinters) {
    this.discovering_ = true;
    this.set(
        'discoveredPrinters', automaticPrinters.concat(discoveredPrinters));
  },

  /** @private */
  onPrinterDiscoveryDone_: function() {
    this.discovering_ = false;
    this.$$('add-printer-list').style.maxHeight = kPrinterListFullHeight + 'px';

    if (!this.discoveredPrinters.length) {
      this.selectedPrinter = getEmptyPrinter_();
      this.fire('no-detected-printer');
    }
  },

  /** @private */
  stopDiscoveringPrinters_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .stopDiscoveringPrinters();
    this.discovering_ = false;
  },

  /** @private */
  switchToManualAddDialog_: function() {
    // We're abandoning discovery in favor of manual specification, so
    // drop the selection if one exists.
    this.selectedPrinter = getEmptyPrinter_();
    this.close();
    this.fire('open-manually-add-printer-dialog');

    if (this.enableUpdatedUi) {
      return;
    }

    this.stopDiscoveringPrinters_();
  },

  /** @private */
  onCancelTap_: function() {
    this.close();

    if (this.enableUpdatedUi) {
      return;
    }

    this.stopDiscoveringPrinters_();
  },

  /** @private */
  switchToConfiguringDialog_: function() {
    this.close();
    this.fire('open-configuring-printer-dialog');

    if (this.enableUpdatedUi) {
      return;
    }

    this.stopDiscoveringPrinters_();
  },

  /**
   * @param {?CupsPrinterInfo} selectedPrinter
   * @return {boolean} Whether the add printer button is enabled.
   * @private
   */
  canAddPrinter_: function(selectedPrinter) {
    return !!selectedPrinter && !!selectedPrinter.printerName;
  },
});

Polymer({
  is: 'add-printer-manually-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {type: Object, notify: true, value: getEmptyPrinter_},

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
     * TODO(jimmyxgong): Remove this feature flag conditional once feature
     * is launched.
     * @private
     */
    enableUpdatedUi: Boolean,
  },

  observers: [
    'printerInfoChanged_(newPrinter.*)',
  ],

  /** @private */
  switchToDiscoveryDialog_: function() {
    this.newPrinter = getEmptyPrinter_();
    this.$$('add-printer-dialog').close();
    this.fire('open-discovery-printers-dialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter success.
   * @param {!PrinterSetupResult} result
   * @private
   * */
  onAddPrinterSucceeded_: function(result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: this.newPrinter.printerName});
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter failure.
   * @param {*} result
   * @private
   * */
  onAddPrinterFailed_: function(result) {
    this.errorText_ = settings.printing.getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  },

  /**
   * Handler for getPrinterInfo success.
   * @param {!PrinterMakeModel} info
   * @private
   * */
  onPrinterFound_: function(info) {
    const newPrinter =
        /** @type {CupsPrinterInfo}  */ (Object.assign({}, this.newPrinter));

    newPrinter.printerManufacturer = info.manufacturer;
    newPrinter.printerModel = info.model;
    newPrinter.printerMakeAndModel = info.makeAndModel;
    newPrinter.printerPpdReference.userSuppliedPpdUrl =
        info.ppdRefUserSuppliedPpdUrl;
    newPrinter.printerPpdReference.effectiveMakeAndModel =
        info.ppdRefEffectiveMakeAndModel;
    newPrinter.printerPpdReference.autoconf = info.autoconf;
    newPrinter.printerPpdReferenceResolved = info.ppdReferenceResolved;

    this.newPrinter = newPrinter;


    // Add the printer if it's configurable. Otherwise, forward to the
    // manufacturer dialog.
    if (this.newPrinter.printerPpdReferenceResolved) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .addCupsPrinter(this.newPrinter)
          .then(
              this.onAddPrinterSucceeded_.bind(this),
              this.onAddPrinterFailed_.bind(this));
    } else {
      this.$$('add-printer-dialog').close();
      this.fire('open-manufacturer-model-dialog');
    }
  },

  /**
   * Handler for getPrinterInfo failure.
   * @param {*} result a PrinterSetupResult with an error code indicating why
   * getPrinterInfo failed.
   * @private
   */
  infoFailed_: function(result) {
    this.addPrinterInProgress_ = false;
    if (result == PrinterSetupResult.PRINTER_UNREACHABLE) {
      this.$.printerAddressInput.invalid = true;
      return;
    }
    this.errorText_ = settings.printing.getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  },

  /** @private */
  addPressed_: function() {
    this.addPrinterInProgress_ = true;

    if (this.newPrinter.printerProtocol == 'ipp' ||
        this.newPrinter.printerProtocol == 'ipps') {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getPrinterInfo(this.newPrinter)
          .then(this.onPrinterFound_.bind(this), this.infoFailed_.bind(this));
    } else {
      this.$$('add-printer-dialog').close();
      this.fire('open-manufacturer-model-dialog');
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_: function(event) {
    this.set('newPrinter.printerProtocol', event.target.value);
  },

  /**
   * @return {boolean} Whether the add printer button is enabled.
   * @private
   */
  canAddPrinter_: function() {
    return !this.addPrinterInProgress_ &&
        settings.printing.isNameAndAddressValid(this.newPrinter);
  },

  /** @private */
  printerInfoChanged_: function() {
    this.$.printerAddressInput.invalid = false;
    this.errorText_ = '';
  },

});

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
     * printer manufacturer. Set by manufacturerDropdown.
     * @private
     */
    isManufacturerInvalid_: Boolean,

    /**
     * Indicates whether the value in the Model dropdown is a valid printer
     * model. Set by modelDropdown.
     * @private
     */
    isModelInvalid_: Boolean,
  },

  observers: [
    'selectedManufacturerChanged_(activePrinter.ppdManufacturer)',
    'selectedModelChanged_(activePrinter.ppdModel)',
  ],

  /** @override */
  attached: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterManufacturersList()
        .then(this.manufacturerListChanged_.bind(this));
  },

  close: function() {
    this.$$('add-printer-dialog').close();
  },

  /**
   * Handler for addCupsPrinter success.
   * @param {!PrinterSetupResult} result
   * @private
   * */
  onPrinterAddedSucceeded_: function(result) {
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: this.activePrinter.printerName});
    this.close();
  },

  /**
   * Handler for addCupsPrinter failure.
   * @param {*} result
   * @private
   * */
  onPrinterAddedFailed_: function(result) {
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
  getManufacturerAndModelSubtext_: function() {
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
  selectedManufacturerChanged_: function(manufacturer) {
    // Reset model if manufacturer is changed.
    this.set('activePrinter.ppdModel', '');
    this.modelList = [];
    if (manufacturer && manufacturer.length != 0) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(manufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /**
   * Attempts to get the EULA Url if the selected printer has one.
   * @private
   */
  selectedModelChanged_: function() {
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
  onGetEulaUrlCompleted_: function(eulaUrl) {
    this.eulaUrl_ = eulaUrl;
  },

  /**
   * @param {!ManufacturersInfo} manufacturersInfo
   * @private
   */
  manufacturerListChanged_: function(manufacturersInfo) {
    if (!manufacturersInfo.success) {
      return;
    }
    this.manufacturerList = manufacturersInfo.manufacturers;
    if (this.activePrinter.ppdManufacturer.length != 0) {
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .getCupsPrinterModelsList(this.activePrinter.ppdManufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  },

  /**
   * @param {!ModelsInfo} modelsInfo
   * @private
   */
  modelListChanged_: function(modelsInfo) {
    if (modelsInfo.success) {
      this.modelList = modelsInfo.models;
    }
  },

  /** @private */
  onBrowseFile_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrinterPPDPath()
        .then(this.printerPPDPathChanged_.bind(this));
  },

  /**
   * @param {string} path The full path to the selected PPD file
   * @private
   */
  printerPPDPathChanged_: function(path) {
    this.set('activePrinter.printerPPDPath', path);
    this.invalidPPD_ = !path;
    this.newUserPPD_ = settings.printing.getBaseName(path);
  },

  /** @private */
  onCancelTap_: function() {
    this.close();
    settings.CupsPrintersBrowserProxyImpl.getInstance().cancelPrinterSetUp(
        this.activePrinter);
  },

  /** @private */
  addPrinter_: function() {
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
  canAddPrinter_: function(ppdManufacturer, ppdModel, printerPPDPath) {
    return !this.addPrinterInProgress_ &&
        settings.printing.isPPDInfoValid(
            ppdManufacturer, ppdModel, printerPPDPath) &&
        !this.isManufacturerInvalid_ && !this.isModelInvalid_;
  },
});

Polymer({
  is: 'add-printer-configuring-dialog',

  properties: {
    printerName: String,
    dialogTitle: String,
  },

  /** @override */
  attached: function() {
    this.$.configuringMessage.textContent =
        loadTimeData.getStringF('printerConfiguringMessage', this.printerName);
  },

  /** @private */
  onCloseConfiguringTap_: function() {
    this.close();
  },

  close: function() {
    this.$$('add-printer-dialog').close();
  },
});

Polymer({
  is: 'settings-cups-add-printer-dialog',

  properties: {
    /** @type {!CupsPrinterInfo} */
    newPrinter: {
      type: Object,
    },

    configuringDialogTitle: String,

    /** @private {string} */
    previousDialog_: String,

    /** @private {string} */
    currentDialog_: String,

    /** @private {boolean} */
    showDiscoveryDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showManuallyAddDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showConfiguringDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showManufacturerDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * TODO(jimmyxgong): Remove this feature flag conditional once feature
     * is launched.
     * @private
     */
    enableUpdatedUi: Boolean,
  },

  listeners: {
    'open-manually-add-printer-dialog': 'openManuallyAddPrinterDialog_',
    'open-configuring-printer-dialog': 'openConfiguringPrinterDialog_',
    'open-discovery-printers-dialog': 'openDiscoveryPrintersDialog_',
    'open-manufacturer-model-dialog':
        'openManufacturerModelDialogForCurrentPrinter_',
    'no-detected-printer': 'onNoDetectedPrinter_',
  },

  /** Opens the Add printer discovery dialog. */
  open: function() {
    this.resetData_();
    if (this.enableUpdatedUi) {
      // The updated UI will remove the discovery dialog. Open the manual
      // dialog by default.
      this.switchDialog_(
          '', AddPrinterDialogs.MANUALLY, 'showManuallyAddDialog_');
    } else {
      this.switchDialog_(
          '', AddPrinterDialogs.DISCOVERY, 'showDiscoveryDialog_');
    }
  },

  /**
   * Reset all the printer data in the Add printer flow.
   * @private
   */
  resetData_: function() {
    if (this.newPrinter) {
      this.newPrinter = getEmptyPrinter_();
    }
  },

  /** @private */
  openManuallyAddPrinterDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUALLY,
        'showManuallyAddDialog_');
  },

  /** @private */
  openDiscoveryPrintersDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.DISCOVERY,
        'showDiscoveryDialog_');
  },

  /** @private */
  switchToManufacturerDialog_: function() {
    this.$$('add-printer-configuring-dialog').close();
    this.openManufacturerModelDialogForCurrentPrinter_();
  },

  /** @private */
  openConfiguringPrinterDialog_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.CONFIGURING,
        'showConfiguringDialog_');
    if (this.previousDialog_ == AddPrinterDialogs.DISCOVERY) {
      this.configuringDialogTitle =
          loadTimeData.getString('addPrintersNearbyTitle');
      settings.CupsPrintersBrowserProxyImpl.getInstance()
          .addDiscoveredPrinter(this.newPrinter.printerId)
          .then(
              this.onAddingDiscoveredPrinterSucceeded_.bind(this),
              this.manuallyAddDiscoveredPrinter_.bind(this));
    } else {
      assertNotReached('Opening configuring dialog from invalid place');
    }
  },

  /** @private */
  openManufacturerModelDialogForCurrentPrinter_: function() {
    this.switchDialog_(
        this.currentDialog_, AddPrinterDialogs.MANUFACTURER,
        'showManufacturerDialog_');
  },

  /** @param {!CupsPrinterInfo} printer */
  openManufacturerModelDialogForSpecifiedPrinter: function(printer) {
    this.newPrinter = printer;
    this.switchDialog_(
        '', AddPrinterDialogs.MANUFACTURER, 'showManufacturerDialog_');
  },

  /** @private */
  onNoDetectedPrinter_: function() {
    // If there is no detected printer, automatically open manually-add-printer
    // dialog only when the user opens the discovery-dialog through the
    // "ADD PRINTER" button.
    if (!this.previousDialog_) {
      this.$$('add-printer-discovery-dialog').close();
      this.newPrinter = getEmptyPrinter_();
      this.openManuallyAddPrinterDialog_();
    }
  },

  /**
   * Switch dialog from |fromDialog| to |toDialog|.
   * @param {string} fromDialog
   * @param {string} toDialog
   * @param {string} domIfBooleanName The name of the boolean variable
   *     corresponding to the |toDialog|.
   * @private
   */
  switchDialog_: function(fromDialog, toDialog, domIfBooleanName) {
    this.previousDialog_ = fromDialog;
    this.currentDialog_ = toDialog;

    this.set(domIfBooleanName, true);
    this.async(function() {
      const dialog = this.$$(toDialog);
      dialog.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
      });
    });
  },

  /**
   * Handler for addDiscoveredPrinter.
   * @param {!PrinterSetupResult} result
   * @private
   * */
  onAddingDiscoveredPrinterSucceeded_: function(result) {
    this.$$('add-printer-configuring-dialog').close();
    this.fire(
        'show-cups-printer-toast',
        {resultCode: result, printerName: this.newPrinter.printerName});
  },

  /**
   * Use the given printer as the starting point for a user-driven
   * add of a printer.  This is called if we can't automatically configure
   * the printer, and need more information from the user.
   *
   * @param {*} printer
   * @private
   */
  manuallyAddDiscoveredPrinter_: function(printer) {
    this.newPrinter = /** @type {CupsPrinterInfo} */ (printer);
    this.switchToManufacturerDialog_();
  },

  /**
   * @param {boolean} success
   * @param {string} printerName
   * @private
   */
  onAddPrinter_: function(success, printerName) {
    // 'on-add-cups-printer' event might be triggered by editing an existing
    // printer, in which case there is no configuring dialog.
    if (this.$$('add-printer-configuring-dialog')) {
      this.$$('add-printer-configuring-dialog').close();
    }
  },
});
