// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
// <if expr="not is_chromeos">
import './destination_list_item.js';
// </if>
// <if expr="is_chromeos">
import './destination_list_item_cros.js';
// </if>
import './print_preview_vars.css.js';
import '../strings.m.js';
import './throbber.css.js';

import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';

import {getTemplate} from './destination_list.html.js';

const DESTINATION_ITEM_HEIGHT = 32;

export interface PrintPreviewDestinationListElement {
  $: {
    list: IronListElement,
  };
}

const PrintPreviewDestinationListElementBase =
    ListPropertyUpdateMixin(PolymerElement);

export class PrintPreviewDestinationListElement extends
    PrintPreviewDestinationListElementBase {
  static get is() {
    return 'print-preview-destination-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      destinations: Array,

      searchQuery: Object,

      loadingDestinations: {
        type: Boolean,
        value: false,
      },

      matchingDestinations_: {
        type: Array,
        value: () => [],
      },

      hasDestinations_: {
        type: Boolean,
        value: true,
      },

      throbberHidden_: {
        type: Boolean,
        value: false,
      },

      hideList_: {
        type: Boolean,
        value: false,
      },
    };
  }

  destinations: Destination[];
  searchQuery: RegExp|null;
  loadingDestinations: boolean;
  private matchingDestinations_: Destination[];
  private hasDestinations_: boolean;
  private throbberHidden_: boolean;
  private hideList_: boolean;

  private boundUpdateHeight_: ((e: Event) => void)|null = null;

  static get observers() {
    return [
      'updateMatchingDestinations_(' +
          'destinations.*, searchQuery, loadingDestinations)',
    ];
  }

  override connectedCallback() {
    super.connectedCallback();

    this.boundUpdateHeight_ = () => this.updateHeight_();
    window.addEventListener('resize', this.boundUpdateHeight_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    window.removeEventListener('resize', this.boundUpdateHeight_!);
    this.boundUpdateHeight_ = null;
  }

  /**
   * This is a workaround to ensure that the iron-list correctly updates the
   * displayed destination information when the elements in the
   * |matchingDestinations_| array change, instead of using stale information
   * (a known iron-list issue). The event needs to be fired while the list is
   * visible, so firing it immediately when the change occurs does not always
   * work.
   */
  private forceIronResize_() {
    this.$.list.fire('iron-resize');
  }

  private updateHeight_(numDestinations?: number) {
    const count = numDestinations === undefined ?
        this.matchingDestinations_.length :
        numDestinations;

    const maxDisplayedItems = this.offsetHeight / DESTINATION_ITEM_HEIGHT;
    const isListFullHeight = maxDisplayedItems <= count;

    // Update the throbber and "No destinations" message.
    this.hasDestinations_ = count > 0 || this.loadingDestinations;
    this.throbberHidden_ =
        !this.loadingDestinations || isListFullHeight || !this.hasDestinations_;

    this.hideList_ = count === 0;
    if (this.hideList_) {
      return;
    }

    const listHeight =
        isListFullHeight ? this.offsetHeight : count * DESTINATION_ITEM_HEIGHT;
    this.$.list.style.height = listHeight > DESTINATION_ITEM_HEIGHT ?
        `${listHeight}px` :
        `${DESTINATION_ITEM_HEIGHT}px`;
  }

  private updateMatchingDestinations_() {
    if (this.destinations === undefined) {
      return;
    }

    const matchingDestinations = this.searchQuery ?
        this.destinations.filter(d => d.matches(this.searchQuery!)) :
        this.destinations.slice();

    // Update the height before updating the list.
    this.updateHeight_(matchingDestinations.length);
    this.updateList(
        'matchingDestinations_', destination => destination.key,
        matchingDestinations);

    this.forceIronResize_();
  }

  private onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.onDestinationSelected_(e);
      e.stopPropagation();
    }
  }

  /**
   * @param e Event containing the destination that was selected.
   */
  private onDestinationSelected_(e: Event) {
    if ((e.composedPath()[0] as HTMLElement).tagName === 'A') {
      return;
    }

    this.dispatchEvent(new CustomEvent(
        'destination-selected',
        {bubbles: true, composed: true, detail: e.target}));
  }

  /**
   * Returns a 1-based index for aria-rowindex.
   */
  private getAriaRowindex_(index: number): number {
    return index + 1;
  }

  // <if expr="is_chromeos">
  updatePrinterStatusIcon(destinationKey: string) {
    const index = this.matchingDestinations_.findIndex(
        destination => destination.key === destinationKey);
    if (index === -1) {
      return;
    }

    this.notifyPath(`matchingDestinations_.${index}.printerStatusReason`);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-list': PrintPreviewDestinationListElement;
  }
}

customElements.define(
    PrintPreviewDestinationListElement.is, PrintPreviewDestinationListElement);
