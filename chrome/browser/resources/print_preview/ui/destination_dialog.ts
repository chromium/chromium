// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../print_preview_utils.js';
import './destination_list.js';
import './print_preview_search_box.js';
import '/strings.m.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';
import type {DestinationStore} from '../data/destination_store.js';
import {DestinationStoreEventType} from '../data/destination_store.js';
import {NativeLayerImpl} from '../native_layer.js';

import {getCss} from './destination_dialog.css.js';
import {getHtml} from './destination_dialog.html.js';
import type {PrintPreviewDestinationListElement} from './destination_list.js';
import type {PrintPreviewSearchBoxElement} from './print_preview_search_box.js';

export interface PrintPreviewDestinationDialogElement {
  $: {
    dialog: CrDialogElement,
    searchBox: PrintPreviewSearchBoxElement,
    printList: PrintPreviewDestinationListElement,
  };
}

export class PrintPreviewDestinationDialogElement extends CrLitElement {
  static get is() {
    return 'print-preview-destination-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      destinationStore: {type: Object},
      loadingDestinations_: {type: Boolean},
      searchQuery_: {type: Object},
    };
  }

  accessor destinationStore: DestinationStore|null = null;
  protected accessor loadingDestinations_: boolean = false;
  protected accessor searchQuery_: RegExp|null = null;

  private tracker_: EventTracker = new EventTracker();
  private initialized_: boolean = false;

  override firstUpdated() {
    this.addEventListener('keydown', (e: KeyboardEvent) => this.onKeydown_(e));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('destinationStore')) {
      this.onDestinationStoreSet_();
    }
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
    assert(!this.initialized_);
    assert(this.destinationStore);
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
    if (!this.destinationStore || !this.initialized_) {
      return;
    }

    this.$.printList.destinations = this.destinationStore.destinations();

    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
  }

  protected onCloseOrCancel_() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
  }

  protected onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  /**
   * @param e Event containing the selected destination.
   */
  protected onDestinationSelected_(e: CustomEvent<Destination>) {
    this.selectDestination_(e.detail);
  }

  private selectDestination_(destination: Destination) {
    assert(this.destinationStore);
    this.destinationStore.selectDestination(destination);
    this.$.dialog.close();
  }

  show() {
    this.$.dialog.showModal();
    const loading = !this.destinationStore ||
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

  protected onManageButtonClick_() {
    NativeLayerImpl.getInstance().managePrinters();
  }

  protected onSearchQueryChanged_(e: CustomEvent<{value: RegExp | null}>) {
    this.searchQuery_ = e.detail.value;
  }
}

export type DestinationDialogElement = PrintPreviewDestinationDialogElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-dialog': PrintPreviewDestinationDialogElement;
  }
}

customElements.define(
    PrintPreviewDestinationDialogElement.is,
    PrintPreviewDestinationDialogElement);
