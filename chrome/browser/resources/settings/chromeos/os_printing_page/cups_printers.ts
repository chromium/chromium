// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers' is a component for showing CUPS
 * Printer settings subpage (chrome://settings/cupsPrinters). It is used to
 * set up legacy & non-CloudPrint printers on ChromeOS by leveraging CUPS (the
 * unix printing system) and the many open source drivers built for CUPS.
 */

// TODO(xdai): Rename it to 'settings-cups-printers-page'.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../../icons.html.js';
import './cups_edit_printer_dialog.js';
import './cups_enterprise_printers.js';
import './cups_printer_shared.css.js';
import './cups_printer_types.js';
import './cups_printers_browser_proxy.js';
import './cups_printers_entry.js';
import './cups_printers_entry_manager.js';
import './cups_saved_printers.js';
import './cups_settings_add_printer_dialog.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {addWebUiListener, removeWebUiListener, WebUiListener} from 'chrome://resources/js/cr.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrosNetworkConfigRemote, FilterType, NetworkStateProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Constructor} from '../common/types.js';
import {DeepLinkingMixin, DeepLinkingMixinInterface} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {PrinterListEntry, PrinterType} from './cups_printer_types.js';
import {getTemplate} from './cups_printers.html.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxyImpl, CupsPrintersList, PrinterSetupResult} from './cups_printers_browser_proxy.js';
import {CupsPrintersEntryManager} from './cups_printers_entry_manager.js';
import {SettingsCupsAddPrinterDialogElement} from './cups_settings_add_printer_dialog.js';

const SettingsCupsPrintersElementBase =
    mixinBehaviors(
        [
          NetworkListenerBehavior,
        ],
        DeepLinkingMixin(
            RouteObserverMixin(WebUiListenerMixin(PolymerElement)))) as
    Constructor<PolymerElement&WebUiListenerMixinInterface&
                RouteObserverMixinInterface&DeepLinkingMixinInterface&
                NetworkListenerBehaviorInterface>;

interface SettingsCupsPrintersElement {
  $: {
    errorToast: CrToastElement,
    printServerErrorToast: CrToastElement,
    addPrinterDialog: SettingsCupsAddPrinterDialogElement,
  };
}

class SettingsCupsPrintersElement extends SettingsCupsPrintersElementBase {
  static get is() {
    return 'settings-cups-printers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      printers: {
        type: Array,
        notify: true,
      },

      prefs: Object,

      activePrinter: {
        type: Object,
        notify: true,
      },

      onPrintersChangedListener_: {
        type: Object,
        value: null,
      },

      onEnterprisePrintersChangedListener_: {
        type: Object,
        value: null,
      },

      searchTerm: {
        type: String,
      },

      /**
       * This is also used as an attribute for css styling.
       */
      canAddPrinter: {
        type: Boolean,
        reflectToAttribute: true,
      },

      savedPrinters_: {
        type: Array,
        value: () => [],
      },

      enterprisePrinters_: {
        type: Array,
        value: () => [],
      },

      attemptedLoadingPrinters_: {
        type: Boolean,
        value: false,
      },

      showCupsEditPrinterDialog_: Boolean,

      addPrinterResultText_: String,

      nearbyPrintersAriaLabel_: {
        type: String,
        computed: 'getNearbyPrintersAriaLabel_(nearbyPrinterCount_)',
      },

      savedPrintersAriaLabel_: {
        type: String,
        computed: 'getSavedPrintersAriaLabel_(savedPrinterCount_)',
      },

      enterprisePrintersAriaLabel_: {
        type: String,
        computed: 'getEnterprisePrintersAriaLabel_(enterprisePrinterCount_)',
      },

      nearbyPrinterCount_: {
        type: Number,
        value: 0,
      },

      savedPrinterCount_: {
        type: Number,
        value: 0,
      },

      enterprisePrinterCount_: {
        type: Number,
        value: 0,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kAddPrinter,
          Setting.kSavedPrinters,
        ]),
      },
    };
  }

  activePrinter: CupsPrinterInfo;
  canAddPrinter: boolean;
  prefs: Object;
  printers: CupsPrinterInfo[];
  searchTerm: string;

  private addPrintServerResultText_: string;
  private addPrinterResultText_: string;
  private attemptedLoadingPrinters_: boolean;
  private enterprisePrinterCount_: number;
  private enterprisePrintersAriaLabel_: string;
  private enterprisePrinters_: PrinterListEntry[];
  private entryManager_: CupsPrintersEntryManager;
  private nearbyPrinterCount_: number;
  private nearbyPrintersAriaLabel_: string;
  private networkConfig_: CrosNetworkConfigRemote;
  private onEnterprisePrintersChangedListener_: WebUiListener;
  private onPrintersChangedListener_: WebUiListener|null;
  private savedPrinterCount_: number;
  private savedPrintersAriaLabel_: string;
  private savedPrinters_: PrinterListEntry[];
  private showCupsEditPrinterDialog_: boolean;

  constructor() {
    super();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();


    this.entryManager_ = CupsPrintersEntryManager.getInstance();

    this.addPrintServerResultText_ = '';
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.networkConfig_
        .getNetworkStateList({
          filter: FilterType.kActive,
          networkType: NetworkType.kAll,
          limit: NO_LIMIT,
        })
        .then((responseParams: {result: NetworkStateProperties[]}) => {
          this.onActiveNetworksChanged(responseParams.result);
        });
  }

  override ready(): void {
    super.ready();

    this.updateCupsPrintersList_();

    this.addEventListener(
        'edit-cups-printer-details', this.onShowCupsEditPrinterDialog_);
    this.addEventListener(
        'show-cups-printer-toast',
        (event: CustomEvent<
            {resultCode: PrinterSetupResult, printerName: string}>) => {
          this.openResultToast_(event);
        });
    this.addEventListener(
        'add-print-server-and-show-toast',
        (event: CustomEvent<{printers: CupsPrintersList}>) => {
          this.addPrintServerAndShowResultToast_((event));
        });
    this.addEventListener(
        'open-manufacturer-model-dialog-for-specified-printer',
        (event: CustomEvent<{item: CupsPrinterInfo}>) => {
          this.openManufacturerModelDialogForSpecifiedPrinter_(event);
        });
  }

  /**
   * Overridden from DeepLinkingMixin.
   */
  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    if (settingId !== Setting.kSavedPrinters) {
      // Continue with deep link attempt.
      return true;
    }

    afterNextRender(this, () => {
      const savedPrinters = this.shadowRoot!.querySelector('#savedPrinters');
      const printerEntry = savedPrinters!.shadowRoot!.querySelector(
          'settings-cups-printers-entry');

      const deepLinkElement =
          printerEntry!.shadowRoot!.querySelector<CrIconButtonElement>(
              '#moreActions');

      if (!deepLinkElement || deepLinkElement.hidden) {
        console.warn(`Element with deep link id ${settingId} not focusable.`);
        return;
      }
      this.showDeepLinkElement(deepLinkElement);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  }

  override currentRouteChanged(route: Route): void {
    if (route !== routes.CUPS_PRINTERS) {
      if (this.onPrintersChangedListener_) {
        removeWebUiListener(this.onPrintersChangedListener_);
        this.onPrintersChangedListener_ = null;
      }
      this.entryManager_.removeWebUiListeners();
      return;
    }

    this.entryManager_.addWebUiListeners();
    this.onPrintersChangedListener_ = addWebUiListener(
        'on-saved-printers-changed', this.onSavedPrintersChanged_.bind(this));
    this.onEnterprisePrintersChangedListener_ = addWebUiListener(
        'on-enterprise-printers-changed',
        this.onEnterprisePrintersChanged_.bind(this));
    this.updateCupsPrintersList_();
    this.attemptDeepLink();
  }

  /**
   * CrosNetworkConfigObserver impl
   */
  override onActiveNetworksChanged(networks: NetworkStateProperties[]): void {
    this.canAddPrinter = networks.some((network) => {
      // Note: Check for kOnline rather than using
      // OncMojo.connectionStateIsConnected() since the latter could return true
      // for networks without connectivity (e.g., captive portals).
      return network.connectionState === ConnectionStateType.kOnline;
    });
  }

  private openResultToast_(
      event:
          CustomEvent<{resultCode: PrinterSetupResult, printerName: string}>):
      void {
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
  }

  private addPrintServerAndShowResultToast_(
      event: CustomEvent<{printers: CupsPrintersList}>): void {
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
  }

  private openManufacturerModelDialogForSpecifiedPrinter_(
      e: CustomEvent<{item: CupsPrinterInfo}>): void {
    const item = e.detail.item;
    this.$.addPrinterDialog.openManufacturerModelDialogForSpecifiedPrinter(
        item);
  }

  private updateCupsPrintersList_(): void {
    CupsPrintersBrowserProxyImpl.getInstance().getCupsSavedPrintersList().then(
        this.onSavedPrintersChanged_.bind(this));

    CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsEnterprisePrintersList()
        .then(this.onEnterprisePrintersChanged_.bind(this));
  }

  private onSavedPrintersChanged_(cupsPrintersList: CupsPrintersList): void {
    this.savedPrinters_ = cupsPrintersList.printerList.map(
        printer => ({printerInfo: printer, printerType: PrinterType.SAVED}));
    this.entryManager_.setSavedPrintersList(this.savedPrinters_);
    // Used to delay rendering nearby and add printer sections to prevent
    // "Add Printer" flicker when clicking "Printers" in settings page.
    this.attemptedLoadingPrinters_ = true;
  }

  private onEnterprisePrintersChanged_(cupsPrintersList: CupsPrintersList):
      void {
    this.enterprisePrinters_ = cupsPrintersList.printerList.map(
        printer =>
            ({printerInfo: printer, printerType: PrinterType.ENTERPRISE}));
    this.entryManager_.setEnterprisePrintersList(this.enterprisePrinters_);
  }

  private onAddPrinterTap_(): void {
    this.$.addPrinterDialog.open();
  }

  private onAddPrinterDialogClose_(): void {
    const icon = this.shadowRoot!.querySelector<CrIconButtonElement>(
        '#addManualPrinterIcon');
    assert(icon);
    focusWithoutInk(icon);
  }

  private onShowCupsEditPrinterDialog_(): void {
    this.showCupsEditPrinterDialog_ = true;
  }

  private onEditPrinterDialogClose_(): void {
    this.showCupsEditPrinterDialog_ = false;
  }

  /**
   * @return Returns if the 'no-search-results-found' string should be shown.
   */
  private showNoSearchResultsMessage_(searchTerm: string): boolean {
    if (!searchTerm || !this.printers.length) {
      return false;
    }
    searchTerm = searchTerm.toLowerCase();
    return !this.printers.some(printer => {
      return printer.printerName.toLowerCase().includes(searchTerm);
    });
  }


  private addPrinterButtonActive_(
      connectedToNetwork: boolean, userPrintersAllowed: boolean): boolean {
    return connectedToNetwork && userPrintersAllowed;
  }

  private doesAccountHaveSavedPrinters_(): boolean {
    return !!this.savedPrinters_.length;
  }

  private doesAccountHaveEnterprisePrinters_(): boolean {
    return !!this.enterprisePrinters_.length;
  }

  private getSavedPrintersAriaLabel_(): string {
    let printerLabel = '';
    if (this.savedPrinterCount_ === 0) {
      printerLabel = 'savedPrintersCountNone';
    } else if (this.savedPrinterCount_ === 1) {
      printerLabel = 'savedPrintersCountOne';
    } else {
      printerLabel = 'savedPrintersCountMany';
    }
    return loadTimeData.getStringF(printerLabel, this.savedPrinterCount_);
  }

  private getNearbyPrintersAriaLabel_(): string {
    let printerLabel = '';
    if (this.nearbyPrinterCount_ === 0) {
      printerLabel = 'nearbyPrintersCountNone';
    } else if (this.nearbyPrinterCount_ === 1) {
      printerLabel = 'nearbyPrintersCountOne';
    } else {
      printerLabel = 'nearbyPrintersCountMany';
    }
    return loadTimeData.getStringF(printerLabel, this.nearbyPrinterCount_);
  }

  private getEnterprisePrintersAriaLabel_(): string {
    let printerLabel = '';
    if (this.enterprisePrinterCount_ === 0) {
      printerLabel = 'enterprisePrintersCountNone';
    } else if (this.enterprisePrinterCount_ === 1) {
      printerLabel = 'enterprisePrintersCountOne';
    } else {
      printerLabel = 'enterprisePrintersCountMany';
    }
    return loadTimeData.getStringF(printerLabel, this.enterprisePrinterCount_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-cups-printers': SettingsCupsPrintersElement;
  }
  interface HTMLElementEventMap {
    'edit-cups-printer-details': CustomEvent;
    'show-cups-printer-toast':
        CustomEvent<{resultCode: PrinterSetupResult, printerName: string}>;
    'add-print-server-and-show-toast':
        CustomEvent<{printers: CupsPrintersList}>;
    'open-manufacturer-model-dialog-for-specified-printer':
        CustomEvent<{item: CupsPrinterInfo}>;
  }
}

customElements.define(
    SettingsCupsPrintersElement.is, SettingsCupsPrintersElement);
