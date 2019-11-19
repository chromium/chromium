// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers' is a component for showing CUPS
 * Printer settings subpage (chrome://settings/cupsPrinters). It is used to
 * set up legacy & non-CloudPrint printers on ChromeOS by leveraging CUPS (the
 * unix printing system) and the many open source drivers built for CUPS.
 */
// TODO(xdai): Rename it to 'settings-cups-printers-page'.
Polymer({
  is: 'settings-cups-printers',

  behaviors: [
      NetworkListenerBehavior,
      settings.RouteObserverBehavior,
      WebUIListenerBehavior,
  ],

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    printers: {
      type: Array,
      notify: true,
    },

    prefs: Object,

    /** @type {?CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },

    searchTerm: {
      type: String,
    },

    /** This is also used as an attribute for css styling. */
    canAddPrinter: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    savedPrinters_: {
      type: Array,
      value: () => [],
    },

    /** @private */
    showCupsEditPrinterDialog_: Boolean,

    /**@private */
    addPrinterResultText_: String,

    /**
     * TODO(jimmyxgong): Remove this feature flag conditional once feature
     * is launched.
     * @private
     */
    enableUpdatedUi_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('updatedCupsPrintersUiEnabled');
      },
    },
  },

  listeners: {
    'edit-cups-printer-details': 'onShowCupsEditPrinterDialog_',
    'show-cups-printer-toast': 'openResultToast_',
    'open-manufacturer-model-dialog-for-specified-printer':
        'openManufacturerModelDialogForSpecifiedPrinter_',
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @private {settings.printing.CupsPrintersEntryManager} */
  entryManager_: null,

  /** @override */
  created: function() {
    this.networkConfig_ =
        network_config.MojoInterfaceProviderImpl.getInstance()
            .getMojoServiceRemote();
    this.entryManager_ =
        settings.printing.CupsPrintersEntryManager.getInstance();
  },

  /** @override */
  attached: function() {
    this.networkConfig_
        .getNetworkStateList({
          filter: chromeos.networkConfig.mojom.FilterType.kActive,
          networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
          limit: chromeos.networkConfig.mojom.NO_LIMIT,
        })
        .then((responseParams) => {
          this.onActiveNetworksChanged(responseParams.result);
        });

    if (this.enableUpdatedUi_) {
      return;
    }
  },

  /** @override */
  ready: function() {
    this.updateCupsPrintersList_();
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route != settings.routes.CUPS_PRINTERS) {
      cr.removeWebUIListener('on-printers-changed');
      this.entryManager_.removeWebUIListeners();
      return;
    }

    this.entryManager_.addWebUIListeners();
    cr.addWebUIListener(
        'on-printers-changed', this.onPrintersChanged_.bind(this));
    this.updateCupsPrintersList_();
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   * @private
   */
  onActiveNetworksChanged: function(networks) {
    this.canAddPrinter = networks.some(function(network) {
      // Note: Check for kOnline rather than using
      // OncMojo.connectionStateIsConnected() since the latter could return true
      // for networks without connectivity (e.g., captive portals).
      return network.connectionState ==
          chromeos.networkConfig.mojom.ConnectionStateType.kOnline;
    });
  },

  /**
   * @param {!CustomEvent<!{
   *      resultCode: PrinterSetupResult,
   *      printerName: string
   * }>} event
   * @private
   */
  openResultToast_: function(event) {
    const printerName = event.detail.printerName;
    switch (event.detail.resultCode) {
      case PrinterSetupResult.SUCCESS:
        this.addPrinterResultText_ =
            loadTimeData.getStringF('printerAddedSuccessfulMessage',
                                    printerName);
        break;
      case PrinterSetupResult.EDIT_SUCCESS:
        this.addPrinterResultText_ =
            loadTimeData.getStringF('printerEditedSuccessfulMessage',
                                    printerName);
        break;
      case PrinterSetupResult.PRINTER_UNREACHABLE:
        if (this.enableUpdatedUi_) {
          this.addPrinterResultText_ =
              loadTimeData.getStringF('printerUnavailableMessage', printerName);
          break;
        }
      default:
        assertNotReached();
      }

    this.$.errorToast.show();
  },

  /**
   * @param {!CustomEvent<{item: !CupsPrinterInfo}>} e
   * @private
   */
  openManufacturerModelDialogForSpecifiedPrinter_: function(e) {
    const item = e.detail.item;
    this.$.addPrinterDialog
        .openManufacturerModelDialogForSpecifiedPrinter(item);
  },

  /** @private */
  updateCupsPrintersList_: function() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrintersList()
        .then(this.onPrintersChanged_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} cupsPrintersList
   * @private
   */
  onPrintersChanged_: function(cupsPrintersList) {
    if (this.enableUpdatedUi_) {
      this.savedPrinters_ = cupsPrintersList.printerList.map(
          printer => /** @type {!PrinterListEntry} */({
              printerInfo: printer,
              printerType: PrinterType.SAVED}));
      this.entryManager_.setSavedPrintersList(this.savedPrinters_);
    } else {
      this.printers = cupsPrintersList.printerList;
    }
  },

  /** @private */
  onAddPrinterTap_: function() {
    this.$.addPrinterDialog.open();
  },

  /** @private */
  onAddPrinterDialogClose_: function() {
      cr.ui.focusWithoutInk(assert(
          this.enableUpdatedUi_ ? this.$$('#addManualPrinterIcon')
                                : this.$$('#addPrinter')));
  },

  /** @private */
  onShowCupsEditPrinterDialog_: function() {
    this.showCupsEditPrinterDialog_ = true;
  },

  /** @private */
  onEditPrinterDialogClose_: function() {
    this.showCupsEditPrinterDialog_ = false;
  },

  /**
   * @param {string} searchTerm
   * @return {boolean} If the 'no-search-results-found' string should be shown.
   * @private
   */
  showNoSearchResultsMessage_: function(searchTerm) {
    if (!searchTerm || !this.printers.length) {
      return false;
    }
    searchTerm = searchTerm.toLowerCase();
    return !this.printers.some(printer => {
      return printer.printerName.toLowerCase().includes(searchTerm);
    });
  },

  /**
   * @param {boolean} connectedToNetwork Whether the device is connected to
         a network.
   * @param {boolean} userNativePrintersAllowed Whether users are allowed to
         configure their own native printers.
   * @return {boolean} Whether the 'Add Printer' button is active.
   * @private
   */
  addPrinterButtonActive_: function(
      connectedToNetwork, userNativePrintersAllowed) {
    return connectedToNetwork && userNativePrintersAllowed;
  },

  /**
   * @return {boolean} Whether |savedPrinters_| is empty.
   * @private
   */
  doesAccountHaveSavedPrinters_: function() {
    return !!this.savedPrinters_.length;
  }
});
