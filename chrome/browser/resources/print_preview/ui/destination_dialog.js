// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
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
import '../strings.m.js';
import './throbber_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {ListPropertyUpdateBehavior, ListPropertyUpdateBehaviorInterface} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {beforeNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {DestinationStore} from '../data/destination_store.js';
import {Metrics, MetricsContext} from '../metrics.js';
import {NativeLayerImpl} from '../native_layer.js';

import {PrintPreviewDestinationListItemElement} from './destination_list_item.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {ListPropertyUpdateBehaviorInterface}
 */
const PrintPreviewDestinationDialogElementBase =
    mixinBehaviors([ListPropertyUpdateBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewDestinationDialogElement extends
    PrintPreviewDestinationDialogElementBase {
  static get is() {
    return 'print-preview-destination-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
    };
  }

  constructor() {
    super();

    /** @private {!EventTracker} */
    this.tracker_ = new EventTracker();

    /** @private {boolean} */
    this.initialized_ = false;
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener(
        'keydown', e => this.onKeydown_(/** @type {!KeyboardEvent} */ (e)));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.tracker_.removeAll();
  }

  /**
   * @param {string} account
   * @private
   */
  fireAccountChange_(account) {
    this.dispatchEvent(new CustomEvent(
        'account-change', {bubbles: true, composed: true, detail: account}));
  }

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
  }

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
  }

  /** @private */
  onActiveUserChange_() {
    if (this.activeUser) {
      this.shadowRoot.querySelector('select').value = this.activeUser;
    }

    this.updateDestinations_();
  }

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
  }

  /**
   * @return {!Array<!Destination>}
   * @private
   */
  getDestinationList_() {
    const destinations = this.destinationStore.destinations(this.activeUser);

    return destinations;
  }

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
      this.fireAccountChange_(this.currentDestinationAccount);
    }
  }

  /** @private */
  onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  /**
   * @param {!CustomEvent<!PrintPreviewDestinationListItemElement>} e Event
   *     containing the selected destination list item element.
   * @private
   */
  onDestinationSelected_(e) {
    const listItem = e.detail;
    const destination = listItem.destination;

    // No provisional or local CrOS destinations on desktop, so all destinations
    // should be ready for selection.
    assert(destination.readyForSelection);
    this.selectDestination_(destination);
  }

  /**
   * @param {!Destination} destination The destination to select.
   * @private
   */
  selectDestination_(destination) {
    this.destinationStore.selectDestination(destination);
    this.$.dialog.close();
  }

  show() {
    if (!this.metrics_) {
      this.metrics_ = MetricsContext.destinationSearch();
    }
    this.$.dialog.showModal();
    this.loadingDestinations_ = this.destinationStore === undefined ||
        this.destinationStore.isPrintDestinationSearchInProgress;
    this.metrics_.record(Metrics.DestinationSearchBucket.DESTINATION_SHOWN);
    if (this.activeUser) {
      beforeNextRender(assert(this.shadowRoot.querySelector('select')), () => {
        this.shadowRoot.querySelector('select').value = this.activeUser;
      });
    }
  }

  /** @return {boolean} Whether the dialog is open. */
  isOpen() {
    return this.$.dialog.hasAttribute('open');
  }

  /** @private */
  onUserChange_() {
    const select = this.shadowRoot.querySelector('select');
    const account = select.value;
    if (account) {
      this.loadingDestinations_ = true;
      this.fireAccountChange_(account);
      this.metrics_.record(Metrics.DestinationSearchBucket.ACCOUNT_CHANGED);
    } else {
      select.value = this.activeUser;
      NativeLayerImpl.getInstance().signIn();
      this.metrics_.record(
          Metrics.DestinationSearchBucket.ADD_ACCOUNT_SELECTED);
    }
  }

  /** @private */
  onManageButtonClick_() {
    this.metrics_.record(Metrics.DestinationSearchBucket.MANAGE_BUTTON_CLICKED);
    NativeLayerImpl.getInstance().managePrinters();
  }
}

customElements.define(
    PrintPreviewDestinationDialogElement.is,
    PrintPreviewDestinationDialogElement);
