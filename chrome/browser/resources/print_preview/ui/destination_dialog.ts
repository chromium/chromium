// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
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
import './destination_list_item.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/js/list_property_update_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {DestinationStore, DestinationStoreEventType} from '../data/destination_store.js';
import {DestinationSearchBucket, MetricsContext} from '../metrics.js';
import {NativeLayerImpl} from '../native_layer.js';

import {getTemplate} from './destination_dialog.html.js';
import {PrintPreviewDestinationListItemElement} from './destination_list_item.js';
import {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';

export interface PrintPreviewDestinationDialogElement {
  $: {
    dialog: CrDialogElement,
    searchBox: PrintPreviewSearchBoxElement,
  };
}

const PrintPreviewDestinationDialogElementBase =
    ListPropertyUpdateMixin(PolymerElement);

export class PrintPreviewDestinationDialogElement extends
    PrintPreviewDestinationDialogElementBase {
  static get is() {
    return 'print-preview-destination-dialog';
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

      destinations_: {
        type: Array,
        value: [],
      },

      loadingDestinations_: {
        type: Boolean,
        value: false,
      },

      metrics_: Object,

      searchQuery_: {
        type: Object,
        value: null,
      },
    };
  }

  destinationStore: DestinationStore;
  private destinations_: Destination[];
  private loadingDestinations_: boolean;
  private metrics_: MetricsContext;
  private searchQuery_: RegExp|null;

  private tracker_: EventTracker = new EventTracker();
  private initialized_: boolean = false;

  override ready() {
    super.ready();
    this.addEventListener('keydown', (e: KeyboardEvent) => this.onKeydown_(e));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.tracker_.removeAll();
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
    const destinations = this.destinationStore.destinations();

    return destinations;
  }

  private onCloseOrCancel_() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    const cancelled = this.$.dialog.getNative().returnValue !== 'success';
    this.metrics_.record(
        cancelled ? DestinationSearchBucket.DESTINATION_CLOSED_UNCHANGED :
                    DestinationSearchBucket.DESTINATION_CLOSED_CHANGED);
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
    this.selectDestination_(destination);
  }

  private selectDestination_(destination: Destination) {
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
    this.metrics_.record(DestinationSearchBucket.DESTINATION_SHOWN);
  }

  /** @return Whether the dialog is open. */
  isOpen(): boolean {
    return this.$.dialog.hasAttribute('open');
  }

  private onManageButtonClick_() {
    this.metrics_.record(DestinationSearchBucket.MANAGE_BUTTON_CLICKED);
    NativeLayerImpl.getInstance().managePrinters();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-dialog': PrintPreviewDestinationDialogElement;
  }
}

customElements.define(
    PrintPreviewDestinationDialogElement.is,
    PrintPreviewDestinationDialogElement);
