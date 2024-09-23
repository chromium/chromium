// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../print_preview_utils.js';
import './destination_dialog_style.css.js';
import './destination_list.js';
import './print_preview_search_box.js';
import './print_preview_shared.css.js';
import './print_preview_vars.css.js';
import '../strings.m.js';
import './throbber.css.js';
import './destination_list_item.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';
import type {DestinationStore} from '../data/destination_store.js';
import {DestinationStoreEventType} from '../data/destination_store.js';
import {NativeLayerImpl} from '../native_layer.js';

import {getTemplate} from './destination_dialog.html.js';
import type {PrintPreviewDestinationListItemElement} from './destination_list_item.js';
import type {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';

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

      searchQuery_: {
        type: Object,
        value: null,
      },
    };
  }

  destinationStore: DestinationStore;
  private destinations_: Destination[];
  private loadingDestinations_: boolean;
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

  private onManageButtonClick_() {
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
