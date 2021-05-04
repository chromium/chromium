// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_searchable_drop_down/cr_searchable_drop_down.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../print_preview_utils.js';
import './destination_dialog_css.js';
import './destination_list.js';
import './print_preview_search_box.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import './provisional_destination_resolver.js';
import '../strings.m.js';
import './throbber_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {beforeNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {DestinationStore} from '../data/destination_store.js';
import {PrintServerStore} from '../data/print_server_store.js';
import {Metrics, MetricsContext} from '../metrics.js';
import {NativeLayerImpl} from '../native_layer.js';
import {PrintServer, PrintServersConfig} from '../native_layer_cros.js';

Polymer({
  is: 'print-preview-destination-dialog-cros',

  _template: html`{__html_template__}`,

  behaviors: [
    ListPropertyUpdateBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {?DestinationStore} */
    destinationStore: {
      type: Object,
      observer: 'onDestinationStoreSet_',
    },

    activeUser: {
      type: String,
      observer: 'onActiveUserChange_',
    },

    currentDestinationAccount: String,

    /** @type {!Array<string>} */
    users: Array,

    /** @private */
    printServerSelected_: {
      type: String,
      value: '',
      observer: 'onPrintServerSelected_',
    },

    /** @private {!Array<!Destination>} */
    destinations_: {
      type: Array,
      value: [],
    },

    /** @private {boolean} */
    loadingDestinations_: {
      type: Boolean,
      value: false,
    },

    /** @private {!MetricsContext} */
    metrics_: Object,

    /** @private {?RegExp} */
    searchQuery_: {
      type: Object,
      value: null,
    },

    /** @private {boolean} */
    isSingleServerFetchingMode_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Array<string>} */
    printServerNames_: {
      type: Array,
      value() {
        return [''];
      },
    },

    /** @private */
    printServerScalingFlagEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('printServerScaling');
      },
      readOnly: true,
    },

    /** @private {boolean} */
    loadingServerPrinters_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    loadingAnyDestinations_: {
      type: Boolean,
      computed:
          'computeLoadingDestinations_(loadingDestinations_, loadingServerPrinters_)'
    },
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private {?Destination} */
  destinationInConfiguring_: null,

  /** @private {boolean} */
  initialized_: false,

  /** @private {?PrintServerStore} */
  printServerStore_: null,

  /** @override */
  detached() {
    this.tracker_.removeAll();
  },

  /** @override */
  ready() {
    if (!this.printServerScalingFlagEnabled_) {
      return;
    }
    this.printServerStore_ = new PrintServerStore(
        (/** string */ eventName, /** !Function */ callback) =>
            void this.addWebUIListener(eventName, callback));
    this.tracker_.add(
        this.printServerStore_,
        PrintServerStore.EventType.PRINT_SERVERS_CHANGED,
        event => void this.onPrintServersChanged_(event));
    this.tracker_.add(
        this.printServerStore_,
        PrintServerStore.EventType.SERVER_PRINTERS_LOADING,
        event => void this.onServerPrintersLoading_(event));
    this.printServerStore_.getPrintServersConfig().then(config => {
      this.printServerNames_ =
          config.printServers.map(printServer => printServer.name);
      this.isSingleServerFetchingMode_ = config.isSingleServerFetchingMode;
    });
    if (this.destinationStore) {
      this.printServerStore_.setDestinationStore(this.destinationStore);
    }
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_(e) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    if (e.key === 'Escape' &&
        (e.composedPath()[0] !== searchInput || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
    }
  },

  /** @private */
  onDestinationStoreSet_() {
    assert(this.destinations_.length === 0);
    const destinationStore = assert(this.destinationStore);
    this.tracker_.add(
        destinationStore, DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDestinations_.bind(this));
    this.tracker_.add(
        destinationStore, DestinationStore.EventType.DESTINATION_SEARCH_DONE,
        this.updateDestinations_.bind(this));
    this.initialized_ = true;
    if (this.printServerStore_) {
      this.printServerStore_.setDestinationStore(this.destinationStore);
    }
  },

  /** @private */
  onActiveUserChange_() {
    if (this.activeUser) {
      this.$$('select').value = this.activeUser;
    }

    this.updateDestinations_();
  },

  /** @private */
  updateDestinations_() {
    if (this.destinationStore === undefined || !this.initialized_) {
      return;
    }

    this.updateList(
        'destinations_', destination => destination.key,
        this.getDestinationList_());

    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
  },

  /**
   * @return {!Array<!Destination>}
   * @private
   */
  getDestinationList_() {
    // Filter out the 'Save to Drive' option so it is not shown in the
    // list of available options.
    return this.destinationStore.destinations(this.activeUser)
        .filter(
            destination =>
                destination.id !== Destination.GooglePromotedId.DOCS &&
                destination.id !==
                    Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS);
  },

  /** @private */
  onCloseOrCancel_() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    const cancelled = this.$.dialog.getNative().returnValue !== 'success';
    this.metrics_.record(
        cancelled ?
            Metrics.DestinationSearchBucket.DESTINATION_CLOSED_UNCHANGED :
            Metrics.DestinationSearchBucket.DESTINATION_CLOSED_CHANGED);
    if (this.currentDestinationAccount &&
        this.currentDestinationAccount !== this.activeUser) {
      this.fire('account-change', this.currentDestinationAccount);
    }
  },

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.cancel();
  },

  /**
   * @param {!CustomEvent<!PrintPreviewDestinationListItemElement>} e Event
   *     containing the selected destination list item element.
   * @private
   */
  onDestinationSelected_(e) {
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
              listItem.onConfigureComplete(response.success);
              if (response.success) {
                destination.capabilities = response.capabilities;
                if (response.policies) {
                  destination.policies = response.policies;
                }
                this.selectDestination_(destination);
                // After destination is selected, start fetching for the EULA
                // URL.
                this.destinationStore.fetchEulaUrl(destination.id);
              }
            },
            () => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(false);
            });
  },

  /**
   * @param {!Destination} destination The destination to select.
   * @private
   */
  selectDestination_(destination) {
    this.destinationStore.selectDestination(destination);
    this.$.dialog.close();
  },

  show() {
    if (!this.metrics_) {
      this.metrics_ = MetricsContext.destinationSearch();
    }
    this.$.dialog.showModal();
    this.loadingDestinations_ = this.destinationStore === undefined ||
        this.destinationStore.isPrintDestinationSearchInProgress;
    this.metrics_.record(Metrics.DestinationSearchBucket.DESTINATION_SHOWN);
    if (this.activeUser) {
      beforeNextRender(assert(this.$$('select')), () => {
        this.$$('select').value = this.activeUser;
      });
    }
  },

  /** @return {boolean} Whether the dialog is open. */
  isOpen() {
    return this.$.dialog.hasAttribute('open');
  },

  /**
   * @param {string} printServerName The name of the print server.
   * @private
   */
  onPrintServerSelected_(printServerName) {
    if (!this.printServerScalingFlagEnabled_ || !this.printServerStore_) {
      return;
    }
    this.printServerStore_.choosePrintServers(printServerName);
  },

  /**
   * @param {!CustomEvent<!{printServerNames: !Array<string>,
   *     isSingleServerFetchingMode: boolean}>} e Event containing the current
   *     print server names and fetching mode.
   * @private
   */
  onPrintServersChanged_(e) {
    this.isSingleServerFetchingMode_ = e.detail.isSingleServerFetchingMode;
    this.printServerNames_ = e.detail.printServerNames;
  },

  /**
   * @param {!CustomEvent<boolean>} e Event containing whether server printers
   *     are currently loading.
   * @private
   */
  onServerPrintersLoading_(e) {
    this.loadingServerPrinters_ = e.detail;
  },

  /**
   * @return {boolean} Whether the destinations are loading.
   * @private
   */
  computeLoadingDestinations_() {
    return this.loadingDestinations_ || this.loadingServerPrinters_;
  },

  /** @private */
  onUserChange_() {
    const select = this.$$('select');
    const account = select.value;
    if (account) {
      this.loadingDestinations_ = true;
      this.fire('account-change', account);
      this.metrics_.record(Metrics.DestinationSearchBucket.ACCOUNT_CHANGED);
    } else {
      select.value = this.activeUser;
      NativeLayerImpl.getInstance().signIn();
      this.metrics_.record(
          Metrics.DestinationSearchBucket.ADD_ACCOUNT_SELECTED);
    }
  },

  /** @private */
  onManageButtonClick_() {
    this.metrics_.record(Metrics.DestinationSearchBucket.MANAGE_BUTTON_CLICKED);
    NativeLayerImpl.getInstance().managePrinters();
  },
});
