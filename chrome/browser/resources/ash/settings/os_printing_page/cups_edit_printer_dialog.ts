// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-edit-printer-dialog' is a dialog to edit the
 * existing printer's information and re-configure it.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_searchable_drop_down/cr_searchable_drop_down.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cups_add_printer_dialog.js';
import './cups_printer_dialog_error.js';
import './cups_printer_shared.css.js';
import './cups_printers_browser_proxy.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {CrosNetworkConfigInterface, FilterType, NetworkStateProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast} from '../assert_extras.js';
import {Constructor} from '../common/types.js';

import {getTemplate} from './cups_edit_printer_dialog.html.js';
import {getBaseName, getErrorText, isNameAndAddressValid, isNetworkProtocol, isPPDInfoValid} from './cups_printer_dialog_util.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, ManufacturersInfo, ModelsInfo, PrinterPpdMakeModel, PrinterSetupResult} from './cups_printers_browser_proxy.js';

/**
 * The types of actions that can be performed with the edit dialog.  These
 * values are written to logs and used as metrics.  New enum values can be
 * added, but existing values must never be renumbered or deleted and reused.
 * See PrinterEditDialogActions enum in tools/metrics/hisograms/enums.xml.
 */
enum DialogActions {
  DIALOG_OPENED = 0,
  VIEW_PPD_CLICKED = 1,
}

/** Keyword used for recording metrics */
const METRICS_KEYWORD = 'Printing.CUPS.PrinterEditDialogActions';

const SettingsCupsEditPrinterDialogElementBase =
    mixinBehaviors([NetworkListenerBehavior], I18nMixin(PolymerElement)) as
    Constructor<PolymerElement&I18nMixinInterface&
                NetworkListenerBehaviorInterface>;

export class SettingsCupsEditPrinterDialogElement extends
    SettingsCupsEditPrinterDialogElementBase {
  static get is() {
    return 'settings-cups-edit-printer-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The currently saved printer.
       */
      activePrinter: Object,

      /**
       * Printer that holds the modified changes to activePrinter and only
       * applies these changes when the save button is clicked.
       */
      pendingPrinter_: Object,

      /**
       * If the printer needs to be re-configured.
       */
      needsReconfigured_: {
        type: Boolean,
        value: false,
      },

      /**
       * The current PPD in use by the printer.
       */
      userPPD_: String,

      /**
       * Tracks whether the dialog is fully initialized. This is required
       * because the dialog isn't fully initialized until Model and Manufacturer
       * are set. Allows us to ignore changes made to these fields until
       * initialization is complete.
       */
      arePrinterFieldsInitialized_: {
        type: Boolean,
        value: false,
      },

      /**
       * If the printer info has changed since loading this dialog. This will
       * only track the freeform input fields, since the other fields contain
       * input selected from dropdown menus.
       */
      printerInfoChanged_: {
        type: Boolean,
        value: false,
      },

      networkProtocolActive_: {
        type: Boolean,
        computed: 'isNetworkProtocol_(pendingPrinter_.printerProtocol)',
      },

      manufacturerList: Array,

      modelList: Array,

      /**
       * Whether the user selected PPD file is valid.
       */
      invalidPPD_: {
        type: Boolean,
        value: false,
      },

      /**
       * The base name of a newly selected PPD file.
       */
      newUserPPD_: String,

      /**
       * The URL to a printer's EULA.
       */
      eulaUrl_: {
        type: String,
        value: '',
      },

      isOnline_: {
        type: Boolean,
        value: true,
      },

      /**
       * The error text to be displayed on the dialog.
       */
      errorText_: {
        type: String,
        value: '',
      },

      /**
       * Indicates whether the value in the Manufacturer dropdown is a valid
       * printer manufacturer.
       */
      isManufacturerInvalid_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether the value in the Model dropdown is a valid printer
       * model.
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

  activePrinter: CupsPrinterInfo;
  manufacturerList: string[];
  modelList: string[];

  private arePrinterFieldsInitialized_: boolean;
  private browserProxy_: CupsPrintersBrowserProxy;
  private errorText_: string;
  private eulaUrl_: string;
  private invalidPPD_: boolean;
  private isManufacturerInvalid_: boolean;
  private isModelInvalid_: boolean;
  private isOnline_: boolean;
  private needsReconfigured_: boolean;
  private networkConfig_: CrosNetworkConfigInterface;
  private networkProtocolActive_: boolean;
  private newUserPPD_: string;
  private pendingPrinter_: CupsPrinterInfo;
  private printerInfoChanged_: boolean;
  private userPPD_: string;

  constructor() {
    super();

    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    chrome.metricsPrivate.recordEnumerationValue(
        METRICS_KEYWORD, DialogActions.DIALOG_OPENED,
        Object.keys(DialogActions).length);

    // Create a copy of activePrinter so that we can modify its fields.
    this.pendingPrinter_ = Object.assign({}, this.activePrinter);

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

  override onActiveNetworksChanged(networks: NetworkStateProperties[]): void {
    this.isOnline_ = networks.some((network) => {
      return OncMojo.connectionStateIsConnected(network.connectionState);
    });
  }

  private printerPathChanged_(change: {path: string, value: string}): void {
    if (change.path !== 'pendingPrinter_.printerName') {
      this.needsReconfigured_ = true;
    }
  }

  private onProtocolChange_(event: Event): void {
    const selectEl = cast(event.target, HTMLSelectElement);
    this.set('pendingPrinter_.printerProtocol', selectEl!.value);
    this.onPrinterInfoChange_();
  }

  private onPrinterInfoChange_(): void {
    this.printerInfoChanged_ = true;
  }

  private onCancelClick_(): void {
    this.shadowRoot!.querySelector('add-printer-dialog')!.close();
  }

  /**
   * Handler for update|reconfigureCupsPrinter success.
   */
  private onPrinterEditSucceeded_(result: PrinterSetupResult): void {
    const showCupsPrinterToastEvent =
        new CustomEvent('show-cups-printer-toast', {
          bubbles: true,
          composed: true,
          detail:
              {resultCode: result, printerName: this.activePrinter.printerName},
        });
    this.dispatchEvent(showCupsPrinterToastEvent);

    this.shadowRoot!.querySelector('add-printer-dialog')!.close();
  }

  /**
   * Handler for update|reconfigureCupsPrinter failure.
   */
  private onPrinterEditFailed_(result: PrinterSetupResult): void {
    this.errorText_ = getErrorText(result);
  }

  private onSaveClick_(): void {
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
  }

  /**
   * @return Returns the i18n string for the dialog title.
   */
  private getDialogTitle_(): string {
    return this.pendingPrinter_.isManaged ?
        this.i18n('viewPrinterDialogTitle') :
        this.i18n('editPrinterDialogTitle');
  }

  private getPrinterUri_(printer: CupsPrinterInfo): string {
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
   */
  private onGetPrinterPpdManufacturerAndModel_(info: PrinterPpdMakeModel):
      void {
    this.set('pendingPrinter_.ppdManufacturer', info.ppdManufacturer);
    this.set('pendingPrinter_.ppdModel', info.ppdModel);

    // |needsReconfigured_| needs to reset to false after |ppdManufacturer| and
    // |ppdModel| are initialized to their correct values.
    this.needsReconfigured_ = false;
  }

  /**
   * Handler for getPrinterPpdManufacturerAndModel() failure case.
   */
  private onGetPrinterPpdManufacturerAndModelFailed_(): void {
    this.needsReconfigured_ = false;
  }

  /**
   * Returns whether |protocol| is a network protocol
   */
  private isNetworkProtocol_(protocol: string): boolean {
    return isNetworkProtocol(protocol);
  }

  /**
   * Returns whether the current printer was auto configured.
   */
  private isAutoconfPrinter_(): boolean {
    return this.pendingPrinter_.printerPpdReference.autoconf;
  }

  /**
   * Returns whether the Save button is enabled.
   */
  private canSavePrinter_(): boolean {
    return this.printerInfoChanged_ &&
        (this.isPrinterConfigured_() || !this.isOnline_) &&
        !this.isManufacturerInvalid_ && !this.isModelInvalid_;
  }

  /**
   * @param manufacturer The manufacturer for which we are retrieving models.
   */
  private selectedEditManufacturerChanged_(manufacturer: string): void {
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
   */
  private onModelChanged_(): void {
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
   * @param eulaUrl The URL for the printer's EULA.
   */
  private onGetEulaUrlCompleted_(eulaUrl: string): void {
    this.eulaUrl_ = eulaUrl;
  }

  private onViewPpd_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        METRICS_KEYWORD, DialogActions.VIEW_PPD_CLICKED,
        Object.keys(DialogActions).length);

    // We always use the activePrinter (the printer when the dialog was first
    // displayed) when viewing the PPD.  Once the user has modified the dialog,
    // the view PPD button is no longer active.
    const eula = this.eulaUrl_ || '';
    this.browserProxy_.retrieveCupsPrinterPpd(
        this.activePrinter.printerId, this.activePrinter.printerName, eula);
  }

  private onBrowseFile_(): void {
    this.browserProxy_.getCupsPrinterPpdPath().then(
        this.printerPpdPathChanged_.bind(this));
  }

  private manufacturerListChanged_(manufacturersInfo: ManufacturersInfo): void {
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

  private modelListChanged_(modelsInfo: ModelsInfo): void {
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
   * @param path The full path to the selected PPD file
   */
  private printerPpdPathChanged_(path: string): void {
    this.set('pendingPrinter_.printerPPDPath', path);
    this.invalidPPD_ = !path;
    if (!this.invalidPPD_) {
      // A new valid PPD file should be treated as a saveable change.
      this.onPrinterInfoChange_();
    }
    this.userPPD_ = getBaseName(path);
  }

  /**
   * @return Returns true if the printer has valid name, address, and valid PPD
   *     or was
   * auto-configured.
   */
  private isPrinterConfigured_(): boolean {
    return isNameAndAddressValid(this.pendingPrinter_) &&
        (this.isAutoconfPrinter_() ||
         isPPDInfoValid(
             this.pendingPrinter_.ppdManufacturer,
             this.pendingPrinter_.ppdModel,
             this.pendingPrinter_.printerPPDPath));
  }

  /**
   * Helper function to copy over modified fields to activePrinter.
   */
  private updateActivePrinter_(): void {
    if (!this.isOnline_) {
      // If we are not online, only copy over the printerName.
      this.activePrinter.printerName = this.pendingPrinter_.printerName;
      return;
    }

    // Clone pendingPrinter_ into activePrinter_.
    this.activePrinter = Object.assign({}, this.pendingPrinter_);

    // Set ppdModel since there is an observer that clears ppdmodel's value when
    // ppdManufacturer changes.
    this.activePrinter.ppdModel = this.pendingPrinter_.ppdModel;
  }

  /**
   * Callback function when networks change.
   */
  private refreshNetworks_(): void {
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
   * @return Returns true if the printer protocol select field should be
   *     enabled.
   */
  private protocolSelectEnabled_(): boolean {
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
   */
  private attemptPpdEulaFetch_(): void {
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
   * @return Returns true if we're on an active network and the printer
   * is not from a print server. If true, the input field is enabled.
   */
  private isInputFieldEnabled_(): boolean {
    // Print server printers should not be editable (except for the name field).
    // Return false to disable the field.
    if (this.pendingPrinter_.printServerUri) {
      return false;
    }

    return this.networkProtocolActive_;
  }

  /**
   * @return Returns true if the printer is managed or not online.
   */
  private isInputFieldReadonly_(): boolean {
    return !this.isOnline_ ||
        (this.pendingPrinter_ && this.pendingPrinter_.isManaged);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cups-edit-printer-dialog': SettingsCupsEditPrinterDialogElement;
  }
}

customElements.define(
    SettingsCupsEditPrinterDialogElement.is,
    SettingsCupsEditPrinterDialogElement);
