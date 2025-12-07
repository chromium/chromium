// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import './destination_list_item.js';
import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';

import {getCss} from './destination_list.css.js';
import {getHtml} from './destination_list.html.js';
import type {PrintPreviewDestinationListItemElement} from './destination_list_item.js';

const DESTINATION_ITEM_HEIGHT: number = 32;

export interface PrintPreviewDestinationListElement {
  $: {
    list: HTMLElement,
  };
}

export class PrintPreviewDestinationListElement extends CrLitElement {
  static get is() {
    return 'print-preview-destination-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      destinations: {type: Array},
      searchQuery: {type: Object},
      loadingDestinations: {type: Boolean},
      matchingDestinations_: {type: Array},
      hasDestinations_: {type: Boolean},
      throbberHidden_: {type: Boolean},
      hideList_: {type: Boolean},
    };
  }

  accessor destinations: Destination[] = [];
  accessor searchQuery: RegExp|null = null;
  accessor loadingDestinations: boolean = false;
  protected accessor matchingDestinations_: Destination[] = [];
  protected accessor hasDestinations_: boolean = true;
  protected accessor throbberHidden_: boolean = false;
  protected accessor hideList_: boolean = false;

  private boundUpdateHeight_: ((e: Event) => void)|null = null;

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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('destinations') ||
        changedProperties.has('searchQuery') ||
        changedProperties.has('loadingDestinations')) {
      this.updateMatchingDestinations_();
    }
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

    this.matchingDestinations_ = matchingDestinations;
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.onDestinationSelected_(e);
      e.stopPropagation();
    }
  }

  /**
   * @param e Event containing the destination that was selected.
   */
  protected onDestinationSelected_(e: Event) {
    if ((e.composedPath()[0] as HTMLElement).tagName === 'A') {
      return;
    }

    const listItem = e.target as PrintPreviewDestinationListItemElement;
    assert(listItem.destination);

    this.dispatchEvent(new CustomEvent(
        'destination-selected',
        {bubbles: true, composed: true, detail: listItem.destination}));
  }

  /**
   * Returns a 1-based index for aria-rowindex.
   */
  protected getAriaRowindex_(index: number): number {
    return index + 1;
  }

  protected onDestinationRowFocus_(e: Event) {
    // Forward focus to the 'print-preview-destination-list-item'.
    const item =
        (e.target as HTMLElement).querySelector<HTMLElement>('.list-item');
    assert(!!item);
    item.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-list': PrintPreviewDestinationListElement;
  }
}

customElements.define(
    PrintPreviewDestinationListElement.is, PrintPreviewDestinationListElement);
