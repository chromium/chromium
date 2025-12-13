// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../print_preview_utils.js';
import './destination_dialog_style.css.js';
import './destination_list.js';
import './print_preview_search_box.js';
import './print_preview_shared.css.js';
import './print_preview_vars.css.js';
import './printer_setup_info_cros.js';
import './provisional_destination_resolver.js';
import '/strings.m.js';
import './throbber.css.js';
import './destination_list_item_cros.js';
import './searchable_drop_down_cros.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement, timeOut} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination_cros.js';
import {GooglePromotedDestinationId} from '../data/destination_cros.js';
import type {DestinationStore} from '../data/destination_store.js';
import {DestinationStoreEventType} from '../data/destination_store.js';
import {PrintServerStore, PrintServerStoreEventType} from '../data/print_server_store.js';
import {MetricsContext, PrintPreviewLaunchSourceBucket} from '../metrics.js';
import {NativeLayerImpl} from '../native_layer.js';
import {NativeLayerCrosImpl} from '../native_layer_cros.js';

import {getTemplate} from './destination_dialog_cros.html.js';
import type {PrintPreviewDestinationListItemElement} from './destination_list_item_cros.js';
import type {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';
import {PrinterSetupInfoInitiator, PrinterSetupInfoMessageType} from './printer_setup_info_cros.js';
import type {PrintPreviewProvisionalDestinationResolverElement} from './provisional_destination_resolver.js';

interface PrintServersChangedEventDetail {
  printServerNames: string[];
  isSingleServerFetchingMode: boolean;
}

// The values correspond to which UI should be currently displayed in the
// dialog.
enum UiState {
  THROBBER = 0,
  DESTINATION_LIST = 1,
  PRINTER_SETUP_ASSISTANCE = 2,
}

export interface PrintPreviewDestinationDialogCrosElement {
  $: {
    dialog: CrDialogElement,
    provisionalResolver: PrintPreviewProvisionalDestinationResolverElement,
    searchBox: PrintPreviewSearchBoxElement,
  };
}

export const DESTINATION_DIALOG_CROS_LOADING_TIMER_IN_MS = 2000;

const PrintPreviewDestinationDialogCrosElementBase =
    ListPropertyUpdateMixin(WebUiListenerMixin(PolymerElement));

export class PrintPreviewDestinationDialogCrosElement extends
    PrintPreviewDestinationDialogCrosElementBase {
  static get is() {
    return 'print-preview-destination-dialog-cros';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      destinationStore: {
        type: Object,
        observer: 'onDestinationStoreSet_',
      },

      printServerSelected_: {
        type: String,
        value: '',
        observer: 'onPrintServerSelected_',
      },

      destinations_: {
        type: Array,
        value: () => [],
      },

      loadingDestinations_: {
        type: Boolean,
        value: false,
      },

      searchQuery_: {
        type: Object,
        value: null,
      },

      isSingleServerFetchingMode_: {
        type: Boolean,
        value: false,
      },

      printServerNames_: {
        type: Array,
        value() {
          return [''];
        },
      },

      loadingServerPrinters_: {
        type: Boolean,
        value: false,
      },

      loadingAnyDestinations_: {
        type: Boolean,
        computed: 'computeLoadingDestinations_(' +
            'loadingDestinations_, loadingServerPrinters_)',
      },

      showManagePrintersButton: {
        type: Boolean,
        reflectToAttribute: true,
      },

      showManagePrinters: Boolean,

      printerSetupInfoMessageTypeEnum_: {
        type: Number,
        value: PrinterSetupInfoMessageType,
        readOnly: true,
      },

      printerSetupInfoInitiatorEnum_: {
        type: Number,
        value: PrinterSetupInfoInitiator,
        readOnly: true,
      },

      minLoadingTimeElapsed_: Boolean,

      uiState_: {
        type: Number,
        value: UiState.THROBBER,
      },

      /**
       * Mirroring the enum so that it can be used from HTML bindings.
       */
      uiStateEnum_: {
        type: Object,
        value: UiState,
      },
    };
  }

  static get observers() {
    return [
      'computeUiState_(minLoadingTimeElapsed_, loadingAnyDestinations_, ' +
          'destinations_.*)',
    ];
  }

  destinationStore: DestinationStore;
  private printServerSelected_: string;
  private destinations_: Destination[];
  private loadingDestinations_: boolean;
  private searchQuery_: RegExp|null;
  private isSingleServerFetchingMode_: boolean;
  private printServerNames_: string[];
  private loadingServerPrinters_: boolean;
  private loadingAnyDestinations_: boolean;
  private metricsContext_: MetricsContext;
  private showManagePrintersButton: boolean;

  private tracker_: EventTracker = new EventTracker();
  private destinationInConfiguring_: Destination|null = null;
  private initialized_: boolean = false;
  private printServerStore_: PrintServerStore|null = null;
  private showManagePrinters: boolean = false;
  private timerDelay_: number = 0;
  private minLoadingTimeElapsed_: boolean = false;
  private uiState_: number = UiState.THROBBER;

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.tracker_.removeAll();
  }

  override ready() {
    super.ready();

    this.addEventListener('keydown', e => this.onKeydown_(e));
    this.printServerStore_ = new PrintServerStore(
        (eventName: string, callback: (p: any) => void) =>
            this.addWebUiListener(eventName, callback));
    this.tracker_.add(
        this.printServerStore_, PrintServerStoreEventType.PRINT_SERVERS_CHANGED,
        this.onPrintServersChanged_.bind(this));
    this.tracker_.add(
        this.printServerStore_,
        PrintServerStoreEventType.SERVER_PRINTERS_LOADING,
        this.onServerPrintersLoading_.bind(this));
    this.printServerStore_.getPrintServersConfig().then(config => {
      this.printServerNames_ =
          config.printServers.map(printServer => printServer.name);
      this.isSingleServerFetchingMode_ = config.isSingleServerFetchingMode;
    });
    if (this.destinationStore) {
      this.printServerStore_.setDestinationStore(this.destinationStore);
    }
    this.metricsContext_ =
        MetricsContext.getLaunchPrinterSettingsMetricsContextCros();
    NativeLayerCrosImpl.getInstance().getShowManagePrinters().then(
        (show: boolean) => {
          this.showManagePrinters = show;
        });
  }

  private onKeydown_(e: KeyboardEvent) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    if (e.key === 'Escape' &&
        (e.composedPath()[0] !== searchInput || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
    }
  }

  private onDestinationStoreSet_() {
    assert(this.destinations_.length === 0);
    this.tracker_.add(
        this.destinationStore, DestinationStoreEventType.DESTINATIONS_INSERTED,
        this.updateDestinations_.bind(this));
    this.tracker_.add(
        this.destinationStore,
        DestinationStoreEventType.DESTINATION_SEARCH_DONE,
        this.updateDestinations_.bind(this));
    this.tracker_.add(
        this.destinationStore,
        DestinationStoreEventType.DESTINATION_PRINTER_STATUS_UPDATE,
        this.onPrinterStatusUpdate_.bind(this));
    this.initialized_ = true;
    if (this.printServerStore_) {
      this.printServerStore_.setDestinationStore(this.destinationStore);
    }
  }

  private updateDestinations_() {
    if (this.destinationStore === undefined || !this.initialized_) {
      return;
    }

    this.updateList(
        'destinations_', destination => destination.key,
        this.getDestinationList_());

    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;

    // Workaround to force the iron-list in print-preview-destination-list to
    // render all destinations and resize to fill dialog body.
    if (this.uiState_ === UiState.DESTINATION_LIST) {
      window.dispatchEvent(new CustomEvent('resize'));
    }
  }

  private getDestinationList_(): Destination[] {
    // Filter out the 'Save to Drive' option so it is not shown in the
    // list of available options.
    return this.destinationStore.destinations().filter(
        destination =>
            destination.id !== GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS);
  }

  private onCloseOrCancel_() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    this.clearThrobberTimer_();
  }

  private onCancelButtonClick_() {
    this.$.dialog.cancel();
    this.clearThrobberTimer_();
  }

  /**
   * @param e Event containing the selected destination list item element.
   */
  private onDestinationSelected_(
      e: CustomEvent<PrintPreviewDestinationListItemElement>) {
    const listItem = e.detail;
    const destination = listItem.destination;

    // ChromeOS local destinations that don't have capabilities need to be
    // configured before selecting, and provisional destinations need to be
    // resolved. Other destinations can be selected.
    if (destination.readyForSelection) {
      this.selectDestination_(destination);
      return;
    }

    // Provisional destinations
    if (destination.isProvisional) {
      this.$.provisionalResolver.resolveDestination(destination)
          .then(this.selectDestination_.bind(this))
          .catch(function() {
            console.warn(
                'Failed to resolve provisional destination: ' + destination.id);
          })
          .then(() => {
            if (this.$.dialog.open && listItem && !listItem.hidden) {
              listItem.focus();
            }
          });
      return;
    }

    // Destination must be a CrOS local destination that needs to be set up.
    // The user is only allowed to set up printer at one time.
    if (this.destinationInConfiguring_) {
      return;
    }

    // Show the configuring status to the user and resolve the destination.
    listItem.onConfigureRequestAccepted();
    this.destinationInConfiguring_ = destination;
    this.destinationStore.resolveCrosDestination(destination)
        .then(
            response => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(true);
              destination.capabilities = response.capabilities;
              // Merge print destination capabilities with the managed print
              // options for that destination if they exist.
              if (loadTimeData.getBoolean(
                      'isUseManagedPrintJobOptionsInPrintPreviewEnabled')) {
                destination.applyAllowedManagedPrintOptions();
              }
              this.selectDestination_(destination);
              // After destination is selected, start fetching for the EULA
              // URL.
              this.destinationStore.fetchEulaUrl(destination.id);
            },
            () => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(false);
            });
  }

  private selectDestination_(destination: Destination) {
    this.destinationStore.selectDestination(destination);
    this.$.dialog.close();
  }

  show() {
    // Before opening the dialog, get the latest destinations from the
    // destination store.
    this.updateDestinations_();

    this.$.dialog.showModal();

    // Workaround to force the iron-list in print-preview-destination-list to
    // render all destinations and resize to fill dialog body.
    if (this.uiState_ === UiState.DESTINATION_LIST) {
      window.dispatchEvent(new CustomEvent('resize'));
    }

    // Display throbber for a minimum period of time while destinations are
    // still loading to avoid empty state UI flashing.
    if (!this.minLoadingTimeElapsed_) {
      this.timerDelay_ = timeOut.run(() => {
        this.minLoadingTimeElapsed_ = true;
      }, DESTINATION_DIALOG_CROS_LOADING_TIMER_IN_MS);
    }
  }

  /** @return Whether the dialog is open. */
  isOpen(): boolean {
    return this.$.dialog.hasAttribute('open');
  }

  private onPrintServerSelected_(printServerName: string) {
    if (!this.printServerStore_) {
      return;
    }
    this.printServerStore_.choosePrintServers(printServerName);
  }

  /**
   * @param e Event containing the current print server names and fetching mode.
   */
  private onPrintServersChanged_(
      e: CustomEvent<PrintServersChangedEventDetail>) {
    this.isSingleServerFetchingMode_ = e.detail.isSingleServerFetchingMode;
    this.printServerNames_ = e.detail.printServerNames;
  }

  /**
   * @param e Event containing whether server printers are currently loading.
   */
  private onServerPrintersLoading_(e: CustomEvent<boolean>) {
    this.loadingServerPrinters_ = e.detail;
  }

  private computeLoadingDestinations_(): boolean {
    return this.loadingDestinations_ || this.loadingServerPrinters_;
  }

  private onManageButtonClick_() {
    NativeLayerImpl.getInstance().managePrinters();
    this.metricsContext_.record(
        PrintPreviewLaunchSourceBucket.DESTINATION_DIALOG_CROS_HAS_PRINTERS);
  }

  private printerDestinationExists(): boolean {
    return this.destinations_.some(
        (destination: Destination): boolean =>
            destination.id !== GooglePromotedDestinationId.SAVE_AS_PDF);
  }

  // Clear throbber timer if it has not completed yet. Used to ensure throbber
  // is shown again if dialog is closed or canceled before throbber is hidden.
  private clearThrobberTimer_(): void {
    if (!this.minLoadingTimeElapsed_) {
      timeOut.cancel(this.timerDelay_);
    }
  }

  // Trigger updates to the printer status icons and text for dialog
  // destinations.
  private onPrinterStatusUpdate_(e: CustomEvent<string>): void {
    const destinationKey = e.detail;
    const destinationList =
        this.shadowRoot!.querySelector('print-preview-destination-list');
    assert(destinationList);
    destinationList.updatePrinterStatusIcon(destinationKey);
  }

  private computeUiState_(): void {
    // First check if the timer is running since the throbber should always show
    // until the timer has elapsed.
    // After the timer has elapsed, if any printers have been detected as
    // available, display them immediately even if searching for more printers.
    // If no printers are available, continue showing the spinner until all
    // printer searches are complete.
    // Once all searches complete and no printers are found then display the
    // Printer Setup Assistance UI.
    if (!this.minLoadingTimeElapsed_) {
      this.uiState_ = UiState.THROBBER;
    } else if (this.printerDestinationExists()) {
      this.uiState_ = UiState.DESTINATION_LIST;

      // Workaround to force the iron-list in print-preview-destination-list to
      // render all destinations and resize to fill dialog body.
      window.dispatchEvent(new CustomEvent('resize'));
      this.$.searchBox.focus();
    } else if (this.loadingAnyDestinations_) {
      this.uiState_ = UiState.THROBBER;
    } else {
      // If there is only a PDF printer, then show Printer Setup Assistance.
      // Save to drive is already excluded by `getDestinationList_()`.
      this.uiState_ = UiState.PRINTER_SETUP_ASSISTANCE;
    }
    this.showManagePrintersButton = this.showManagePrinters &&
        this.uiState_ !== UiState.PRINTER_SETUP_ASSISTANCE;
  }

  private shouldShowUi_(uiState: UiState) {
    return this.uiState_ === uiState;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-dialog-cros':
        PrintPreviewDestinationDialogCrosElement;
  }
}

customElements.define(
    PrintPreviewDestinationDialogCrosElement.is,
    PrintPreviewDestinationDialogCrosElement);
