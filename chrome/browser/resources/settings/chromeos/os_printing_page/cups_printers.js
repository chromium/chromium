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
    DeepLinkingBehavior,
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

    /** @private {?WebUIListener} */
    onPrintersChangedListener_: {
      type: Object,
      value: null,
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
    attemptedLoadingPrinters_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showCupsEditPrinterDialog_: Boolean,

    /**@private */
    addPrinterResultText_: String,

    /**@private */
    nearbyPrintersAriaLabel_: {
      type: String,
      computed: 'getNearbyPrintersAriaLabel_(nearbyPrinterCount_)',
    },

    /**@private */
    savedPrintersAriaLabel_: {
      type: String,
      computed: 'getSavedPrintersAriaLabel_(savedPrinterCount_)',
    },

    nearbyPrinterCount_: {
      type: Number,
      value: 0,
    },

    savedPrinterCount_: {
      type: Number,
      value: 0,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kAddPrinter,
        chromeos.settings.mojom.Setting.kSavedPrinters,
      ]),
    },
  },

  listeners: {
    'edit-cups-printer-details': 'onShowCupsEditPrinterDialog_',
    'show-cups-printer-toast': 'openResultToast_',
    'add-print-server-and-show-toast': 'addPrintServerAndShowResultToast_',
    'open-manufacturer-model-dialog-for-specified-printer':
        'openManufacturerModelDialogForSpecifiedPrinter_',
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @private {settings.printing.CupsPrintersEntryManager} */
  entryManager_: null,

  /** @override */
  created() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
    this.entryManager_ =
        settings.printing.CupsPrintersEntryManager.getInstance();
  },

  /** @override */
  attached() {
    this.networkConfig_
        .getNetworkStateList({
          filter: chromeos.networkConfig.mojom.FilterType.kActive,
          networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
          limit: chromeos.networkConfig.mojom.NO_LIMIT,
        })
        .then((responseParams) => {
          this.onActiveNetworksChanged(responseParams.result);
        });
  },

  /** @override */
  ready() {
    this.updateCupsPrintersList_();
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    // Manually show the deep links for settings nested within elements.
    if (settingId !== chromeos.settings.mojom.Setting.kSavedPrinters) {
      // Continue with deep link attempt.
      return true;
    }

    Polymer.RenderStatus.afterNextRender(this, () => {
      const savedPrinters = this.$$('#savedPrinters');
      const printerEntry =
          savedPrinters && savedPrinters.$$('settings-cups-printers-entry');
      const deepLinkElement = printerEntry && printerEntry.$$('#moreActions');
      if (!deepLinkElement || deepLinkElement.hidden) {
        console.warn(`Element with deep link id ${settingId} not focusable.`);
        return;
      }
      this.showDeepLinkElement(deepLinkElement);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged(route) {
    if (route != settings.routes.CUPS_PRINTERS) {
      if (this.onPrintersChangedListener_) {
        cr.removeWebUIListener(
            /** @type {WebUIListener} */ (this.onPrintersChangedListener_));
        this.onPrintersChangedListener_ = null;
      }
      this.entryManager_.removeWebUIListeners();
      return;
    }

    this.entryManager_.addWebUIListeners();
    this.onPrintersChangedListener_ = cr.addWebUIListener(
        'on-printers-changed', this.onPrintersChanged_.bind(this));
    this.updateCupsPrintersList_();
    this.attemptDeepLink();
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   * @private
   */
  onActiveNetworksChanged(networks) {
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
  openResultToast_(event) {
    const printerName = event.detail.printerName;
    switch (event.detail.resultCode) {
      case PrinterSetupResult.SUCCESS:
        this.addPrinterResultText_ = loadTimeData.getStringF(
            'printerAddedSuccessfulMessage', printerName);
        break;
      case PrinterSetupResult.EDIT_SUCCESS:
        this.addPrinterResultText_ = loadTimeData.getStringF(
            'printerEditedSuccessfulMessage', printerName);
        break;
      case PrinterSetupResult.PRINTER_UNREACHABLE:
        this.addPrinterResultText_ =
            loadTimeData.getStringF('printerUnavailableMessage', printerName);
        break;
      default:
        assertNotReached();
    }

    this.$.errorToast.show();
  },

  /**
   * @param {!CustomEvent<!{
   *      printers: !CupsPrintersList
   * }>} event
   * @private
   */
  addPrintServerAndShowResultToast_: function(event) {
    this.entryManager_.addPrintServerPrinters(event.detail.printers);
    const length = event.detail.printers.printerList.length;
    if (length === 0) {
      this.addPrintServerResultText_ =
          loadTimeData.getString('printServerFoundZeroPrinters');
    } else if (length === 1) {
      this.addPrintServerResultText_ =
          loadTimeData.getString('printServerFoundOnePrinter');
    } else {
      this.addPrintServerResultText_ =
          loadTimeData.getStringF('printServerFoundManyPrinters', length);
    }
    this.$.printServerErrorToast.show();
  },

  /**
   * @param {!CustomEvent<{item: !CupsPrinterInfo}>} e
   * @private
   */
  openManufacturerModelDialogForSpecifiedPrinter_(e) {
    const item = e.detail.item;
    this.$.addPrinterDialog.openManufacturerModelDialogForSpecifiedPrinter(
        item);
  },

  /** @private */
  updateCupsPrintersList_() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsPrintersList()
        .then(this.onPrintersChanged_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} cupsPrintersList
   * @private
   */
  onPrintersChanged_(cupsPrintersList) {
    this.savedPrinters_ = cupsPrintersList.printerList.map(
        printer => /** @type {!PrinterListEntry} */ (
            {printerInfo: printer, printerType: PrinterType.SAVED}));
    this.entryManager_.setSavedPrintersList(this.savedPrinters_);
    // Used to delay rendering nearby and add printer sections to prevent
    // "Add Printer" flicker when clicking "Printers" in settings page.
    this.attemptedLoadingPrinters_ = true;
  },

  /** @private */
  onAddPrinterTap_() {
    this.$.addPrinterDialog.open();
  },

  /** @private */
  onAddPrinterDialogClose_() {
    cr.ui.focusWithoutInk(assert(this.$$('#addManualPrinterIcon')));
  },

  /** @private */
  onShowCupsEditPrinterDialog_() {
    this.showCupsEditPrinterDialog_ = true;
  },

  /** @private */
  onEditPrinterDialogClose_() {
    this.showCupsEditPrinterDialog_ = false;
  },

  /**
   * @param {string} searchTerm
   * @return {boolean} If the 'no-search-results-found' string should be shown.
   * @private
   */
  showNoSearchResultsMessage_(searchTerm) {
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
   * @param {boolean} userPrintersAllowed Whether users are allowed to
         configure their own native printers.
   * @return {boolean} Whether the 'Add Printer' button is active.
   * @private
   */
  addPrinterButtonActive_(connectedToNetwork, userPrintersAllowed) {
    return connectedToNetwork && userPrintersAllowed;
  },

  /**
   * @return {boolean} Whether |savedPrinters_| is empty.
   * @private
   */
  doesAccountHaveSavedPrinters_() {
    return !!this.savedPrinters_.length;
  },

  /** @private */
  getSavedPrintersAriaLabel_() {
    const printerLabel = this.savedPrinterCount_ == 0 ?
        'savedPrintersCountNone' :
        this.savedPrinterCount_ == 1 ? 'savedPrintersCountOne' :
                                       'savedPrintersCountMany';

    return loadTimeData.getStringF(printerLabel, this.savedPrinterCount_);
  },

  /** @private */
  getNearbyPrintersAriaLabel_() {
    const printerLabel = this.nearbyPrinterCount_ == 0 ?
        'nearbyPrintersCountNone' :
        this.nearbyPrinterCount_ == 1 ? 'nearbyPrintersCountOne' :
                                        'nearbyPrintersCountMany';

    return loadTimeData.getStringF(printerLabel, this.nearbyPrinterCount_);
  },
});
