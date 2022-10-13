// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-edit-printer-dialog' is a dialog to edit the
 * existing printer's information and re-configure it.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_searchable_drop_down/cr_searchable_drop_down.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './cups_add_printer_dialog.js';
import './cups_printer_dialog_error.js';
import './cups_printer_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote, FilterType, NetworkStateProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {getBaseName, getErrorText, isNameAndAddressValid, isNetworkProtocol, isPPDInfoValid} from './cups_printer_dialog_util.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, ManufacturersInfo, ModelsInfo, PrinterPpdMakeModel, PrinterSetupResult} from './cups_printers_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {NetworkListenerBehaviorInterface}
 */
const SettingsCupsEditPrinterDialogElementBase =
    mixinBehaviors([I18nBehavior, NetworkListenerBehavior], PolymerElement);

/** @polymer */
class SettingsCupsEditPrinterDialogElement extends
    SettingsCupsEditPrinterDialogElementBase {
  static get is() {
    return 'settings-cups-edit-printer-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The currently saved printer.
       * @type {CupsPrinterInfo}
       */
      activePrinter: Object,

      /**
       * Printer that holds the modified changes to activePrinter and only
       * applies these changes when the save button is clicked.
       * @type {CupsPrinterInfo}
       */
      pendingPrinter_: Object,

      /**
       * If the printer needs to be re-configured.
       * @private {boolean}
       */
      needsReconfigured_: {
        type: Boolean,
        value: false,
      },

      /**
       * The current PPD in use by the printer.
       * @private
       */
      userPPD_: String,

      /**
       * Tracks whether the dialog is fully initialized. This is required
       * because the dialog isn't fully initialized until Model and Manufacturer
       * are set. Allows us to ignore changes made to these fields until
       * initialization is complete.
       * @private
       */
      arePrinterFieldsInitialized_: {
        type: Boolean,
        value: false,
      },

      /**
       * If the printer info has changed since loading this dialog. This will
       * only track the freeform input fields, since the other fields contain
       * input selected from dropdown menus.
       * @private
       */
      printerInfoChanged_: {
        type: Boolean,
        value: false,
      },

      networkProtocolActive_: {
        type: Boolean,
        computed: 'isNetworkProtocol_(pendingPrinter_.printerProtocol)',
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
      isOnline_: {
        type: Boolean,
        value: true,
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
      isManufacturerInvalid_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether the value in the Model dropdown is a valid printer
       * model.
       * @private
       */
      isModelInvalid_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'printerPathChanged_(pendingPrinter_.*)',
      'selectedEditManufacturerChanged_(pendingPrinter_.ppdManufacturer)',
      'onModelChanged_(pendingPrinter_.ppdModel)',
    ];
  }

  /** @override */
  constructor() {
    super();

    /** @private {!CupsPrintersBrowserProxy} */
    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();

    /** @private {!CrosNetworkConfigRemote} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    // Create a copy of activePrinter so that we can modify its fields.
    this.pendingPrinter_ = /** @type{CupsPrinterInfo} */
        (Object.assign({}, this.activePrinter));

    this.refreshNetworks_();

    this.browserProxy_
        .getPrinterPpdManufacturerAndModel(this.pendingPrinter_.printerId)
        .then(
            this.onGetPrinterPpdManufacturerAndModel_.bind(this),
            this.onGetPrinterPpdManufacturerAndModelFailed_.bind(this));
    this.browserProxy_.getCupsPrinterManufacturersList().then(
        this.manufacturerListChanged_.bind(this));
    this.userPPD_ = getBaseName(this.pendingPrinter_.printerPPDPath);
  }

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<NetworkStateProperties>}
   *     networks
   * @private
   */
  onActiveNetworksChanged(networks) {
    this.isOnline_ = networks.some(function(network) {
      return OncMojo.connectionStateIsConnected(network.connectionState);
    });
  }

  /**
   * @param {!{path: string, value: string}} change
   * @private
   */
  printerPathChanged_(change) {
    if (change.path !== 'pendingPrinter_.printerName') {
      this.needsReconfigured_ = true;
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onProtocolChange_(event) {
    this.set('pendingPrinter_.printerProtocol', event.target.value);
    this.onPrinterInfoChange_();
  }

  /** @private */
  onPrinterInfoChange_() {
    this.printerInfoChanged_ = true;
  }

  /** @private */
  onCancelTap_() {
    this.shadowRoot.querySelector('add-printer-dialog').close();
  }

  /**
   * Handler for update|reconfigureCupsPrinter success.
   * @param {!PrinterSetupResult} result
   * @private
   */
  onPrinterEditSucceeded_(result) {
    const showCupsPrinterToastEvent =
        new CustomEvent('show-cups-printer-toast', {
          bubbles: true,
          composed: true,
          detail:
              {resultCode: result, printerName: this.activePrinter.printerName},
        });
    this.dispatchEvent(showCupsPrinterToastEvent);

    this.shadowRoot.querySelector('add-printer-dialog').close();
  }

  /**
   * Handler for update|reconfigureCupsPrinter failure.
   * @param {*} result
   * @private
   */
  onPrinterEditFailed_(result) {
    this.errorText_ = getErrorText(
        /** @type {PrinterSetupResult} */ (result));
  }

  /** @private */
  onSaveTap_() {
    this.updateActivePrinter_();
    if (!this.needsReconfigured_ || !this.isOnline_) {
      // If we don't need to reconfigure or we are offline, just update the
      // printer name.
      this.browserProxy_
          .updateCupsPrinter(
              this.activePrinter.printerId, this.activePrinter.printerName)
          .then(
              this.onPrinterEditSucceeded_.bind(this),
              this.onPrinterEditFailed_.bind(this));
    } else {
      this.browserProxy_.reconfigureCupsPrinter(this.activePrinter)
          .then(
              this.onPrinterEditSucceeded_.bind(this),
              this.onPrinterEditFailed_.bind(this));
    }
    recordSettingChange();
  }

  /**
   * @return {string} The i18n string for the dialog title.
   * @private
   */
  getDialogTitle_() {
    return this.pendingPrinter_.isManaged ?
        this.i18n('viewPrinterDialogTitle') :
        this.i18n('editPrinterDialogTitle');
  }

  /**
   * @param {!CupsPrinterInfo} printer
   * @return {string} The printer's URI that displays in the UI
   * @private
   */
  getPrinterURI_(printer) {
    if (!printer) {
      return '';
    } else if (
        printer.printerProtocol && printer.printerAddress &&
        printer.printerQueue) {
      return printer.printerProtocol + '://' + printer.printerAddress + '/' +
          printer.printerQueue;
    } else if (printer.printerProtocol && printer.printerAddress) {
      return printer.printerProtocol + '://' + printer.printerAddress;
    } else {
      return '';
    }
  }

  /**
   * Handler for getPrinterPpdManufacturerAndModel() success case.
   * @param {!PrinterPpdMakeModel} info
   * @private
   */
  onGetPrinterPpdManufacturerAndModel_(info) {
    this.set('pendingPrinter_.ppdManufacturer', info.ppdManufacturer);
    this.set('pendingPrinter_.ppdModel', info.ppdModel);

    // |needsReconfigured_| needs to reset to false after |ppdManufacturer| and
    // |ppdModel| are initialized to their correct values.
    this.needsReconfigured_ = false;
  }

  /**
   * Handler for getPrinterPpdManufacturerAndModel() failure case.
   * @private
   */
  onGetPrinterPpdManufacturerAndModelFailed_() {
    this.needsReconfigured_ = false;
  }

  /**
   * @param {string} protocol
   * @return {boolean} Whether |protocol| is a network protocol
   * @private
   */
  isNetworkProtocol_(protocol) {
    return isNetworkProtocol(protocol);
  }

  /**
   * @return {boolean} Whether the current printer was auto configured.
   * @private
   */
  isAutoconfPrinter_() {
    return this.pendingPrinter_.printerPpdReference.autoconf;
  }

  /**
   * @return {boolean} Whether the Save button is enabled.
   * @private
   */
  canSavePrinter_() {
    return this.printerInfoChanged_ &&
        (this.isPrinterConfigured_() || !this.isOnline_) &&
        !this.isManufacturerInvalid_ && !this.isModelInvalid_;
  }

  /**
   * @param {string} manufacturer The manufacturer for which we are retrieving
   *     models.
   * @private
   */
  selectedEditManufacturerChanged_(manufacturer) {
    // Reset model if manufacturer is changed.
    this.set('pendingPrinter_.ppdModel', '');
    this.modelList = [];
    if (!!manufacturer && manufacturer.length !== 0) {
      this.browserProxy_.getCupsPrinterModelsList(manufacturer)
          .then(this.modelListChanged_.bind(this));
    }
  }

  /**
   * Sets printerInfoChanged_ to true to show that the model has changed. Also
   * attempts to get the EULA Url if the selected printer has one.
   * @private
   */
  onModelChanged_() {
    if (this.arePrinterFieldsInitialized_) {
      this.printerInfoChanged_ = true;
    }

    if (!this.pendingPrinter_.ppdManufacturer ||
        !this.pendingPrinter_.ppdModel) {
      // Do not check for an EULA unless both |ppdManufacturer| and |ppdModel|
      // are set. Set |eulaUrl_| to be empty in this case.
      this.onGetEulaUrlCompleted_('' /* eulaUrl */);
      return;
    }

    this.attemptPpdEulaFetch_();
  }

  /**
   * @param {string} eulaUrl The URL for the printer's EULA.
   * @private
   */
  onGetEulaUrlCompleted_(eulaUrl) {
    this.eulaUrl_ = eulaUrl;
  }

  /** @private */
  onBrowseFile_() {
    this.browserProxy_.getCupsPrinterPPDPath().then(
        this.printerPPDPathChanged_.bind(this));
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
    if (this.pendingPrinter_.ppdManufacturer.length !== 0) {
      this.browserProxy_
          .getCupsPrinterModelsList(this.pendingPrinter_.ppdManufacturer)
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
      // ModelListChanged_ is the final step of initializing pendingPrinter.
      this.arePrinterFieldsInitialized_ = true;

      // Fetch the EULA URL once we have PpdReferences from fetching the
      // |modelList|.
      this.attemptPpdEulaFetch_();
    }
  }

  /**
   * @param {string} path The full path to the selected PPD file
   * @private
   */
  printerPPDPathChanged_(path) {
    this.set('pendingPrinter_.printerPPDPath', path);
    this.invalidPPD_ = !path;
    if (!this.invalidPPD_) {
      // A new valid PPD file should be treated as a saveable change.
      this.onPrinterInfoChange_();
    }
    this.userPPD_ = getBaseName(path);
  }

  /**
   * Returns true if the printer has valid name, address, and valid PPD or was
   * auto-configured.
   * @return {boolean}
   * @private
   */
  isPrinterConfigured_() {
    return isNameAndAddressValid(this.pendingPrinter_) &&
        (this.isAutoconfPrinter_() ||
         isPPDInfoValid(
             this.pendingPrinter_.ppdManufacturer,
             this.pendingPrinter_.ppdModel,
             this.pendingPrinter_.printerPPDPath));
  }

  /**
   * Helper function to copy over modified fields to activePrinter.
   * @private
   */
  updateActivePrinter_() {
    if (!this.isOnline_) {
      // If we are not online, only copy over the printerName.
      this.activePrinter.printerName = this.pendingPrinter_.printerName;
      return;
    }

    this.activePrinter =
        /** @type{CupsPrinterInfo} */ (Object.assign({}, this.pendingPrinter_));
    // Set ppdModel since there is an observer that clears ppdmodel's value when
    // ppdManufacturer changes.
    this.activePrinter.ppdModel = this.pendingPrinter_.ppdModel;
  }

  /**
   * Callback function when networks change.
   * @private
   */
  refreshNetworks_() {
    this.networkConfig_
        .getNetworkStateList({
          filter: FilterType.kActive,
          networkType: NetworkType.kAll,
          limit: NO_LIMIT,
        })
        .then((responseParams) => {
          this.onActiveNetworksChanged(responseParams.result);
        });
  }

  /**
   * Returns true if the printer protocol select field should be enabled.
   * @return {boolean}
   * @private
   */
  protocolSelectEnabled_() {
    if (this.pendingPrinter_) {
      // Print server printer's protocol should not be editable; disable the
      // drop down if the printer is from a print server.
      if (this.pendingPrinter_.printServerUri) {
        return false;
      }

      // Managed printers are not editable.
      if (this.pendingPrinter_.isManaged) {
        return false;
      }
    }

    return this.isOnline_ && this.networkProtocolActive_;
  }

  /**
   * Attempts fetching for the EULA Url based off of the current printer's
   * |ppdManufacturer| and |ppdModel|.
   * @private
   */
  attemptPpdEulaFetch_() {
    if (!this.pendingPrinter_.ppdManufacturer ||
        !this.pendingPrinter_.ppdModel) {
      return;
    }

    this.browserProxy_
        .getEulaUrl(
            this.pendingPrinter_.ppdManufacturer, this.pendingPrinter_.ppdModel)
        .then(this.onGetEulaUrlCompleted_.bind(this));
  }

  /**
   * @return {boolean} True if we're on an active network and the printer
   * is not from a print server. If true, the input field is enabled.
   * @private
   */
  isInputFieldEnabled_() {
    // Print server printers should not be editable (except for the name field).
    // Return false to disable the field.
    if (this.pendingPrinter_.printServerUri) {
      return false;
    }

    return this.networkProtocolActive_;
  }

  /**
   * @return {boolean} True if the printer is managed or not online.
   * @private
   */
  isInputFieldReadonly_() {
    return !this.isOnline_ ||
        (this.pendingPrinter_ && this.pendingPrinter_.isManaged);
  }
}

customElements.define(
    SettingsCupsEditPrinterDialogElement.is,
    SettingsCupsEditPrinterDialogElement);
