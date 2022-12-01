// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_searchable_drop_down/cr_searchable_drop_down.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../print_preview_utils.js';
import './destination_dialog_style.css.js';
import './destination_list.js';
import './print_preview_search_box.js';
import './print_preview_shared.css.js';
import './print_preview_vars.css.js';
import './provisional_destination_resolver.js';
import '../strings.m.js';
import './throbber.css.js';
import './destination_list_item_cros.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, GooglePromotedDestinationId} from '../data/destination.js';
import {DestinationStore, DestinationStoreEventType} from '../data/destination_store.js';
import {PrintServerStore, PrintServerStoreEventType} from '../data/print_server_store.js';
import {NativeLayerImpl} from '../native_layer.js';

import {getTemplate} from './destination_dialog_cros.html.js';
import {PrintPreviewDestinationListItemElement} from './destination_list_item_cros.js';
import {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';
import {PrintPreviewProvisionalDestinationResolverElement} from './provisional_destination_resolver.js';

interface PrintServersChangedEventDetail {
  printServerNames: string[];
  isSingleServerFetchingMode: boolean;
}

export interface PrintPreviewDestinationDialogCrosElement {
  $: {
    dialog: CrDialogElement,
    provisionalResolver: PrintPreviewProvisionalDestinationResolverElement,
    searchBox: PrintPreviewSearchBoxElement,
  };
}

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
    };
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

  private tracker_: EventTracker = new EventTracker();
  private destinationInConfiguring_: Destination|null = null;
  private initialized_: boolean = false;
  private printServerStore_: PrintServerStore|null = null;

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.tracker_.removeAll();
  }

  override ready() {
    super.ready();

    this.addEventListener('keydown', e => this.onKeydown_(e as KeyboardEvent));
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
  }

  private onCancelButtonClick_() {
    this.$.dialog.cancel();
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
    this.$.dialog.showModal();
    const loading = this.destinationStore === undefined ||
        this.destinationStore.isPrintDestinationSearchInProgress;
    if (!loading) {
      // All destinations have already loaded.
      this.updateDestinations_();
    }
    this.loadingDestinations_ = loading;
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
