// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './organizer_list_section_item.js';

import {assert} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section.css.js';
import {getHtml} from './organizer_list_section.html.js';
import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from './organizer_list_section_delegate.js';
import type {OrganizerListSectionItem, OrganizerListSectionItemElement} from './organizer_list_section_item.js';

/**
 * This is the number of items in a section that are rendered before the "Show
 * more" option.
 */
export const INITIAL_ITEM_COUNT = 3;

export interface OrganizerListSectionElement {
  $: {
    header: HTMLElement,
    items: HTMLElement,
  };
}

export class OrganizerListSectionElement extends CrLitElement implements
    OrganizerListSectionClient {
  static get is() {
    return 'organizer-list-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},
      items: {type: Array},
      expanded_: {type: Boolean},
      searchQuery: {type: String},
    };
  }

  accessor delegate: OrganizerListSectionDelegate<unknown>|null = null;
  accessor items: Array<OrganizerListSectionItem<unknown>> = [];
  protected accessor expanded_: boolean = false;
  accessor searchQuery: string = '';

  // The panel WebUI will remain loaded but invisible when the panel is closed.
  // While invisible, the WebUI will not receive update events from the browser,
  // so all sections need to refetch their data upon visibility change.
  private onVisibilityChange_: () => void = () => {
    if (document.visibilityState === 'visible') {
      this.updateItems_();
    }
  };

  override connectedCallback() {
    super.connectedCallback();
    document.addEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('delegate')) {
      this.delegate?.init(this);
      this.updateItems_();
    }
  }

  onItemsChanged(items: Array<OrganizerListSectionItem<unknown>>) {
    this.items = items;
  }

  private async updateItems_() {
    if (!this.delegate) {
      this.items = [];
      return;
    }
    this.items = await this.delegate.getItems();
  }

  protected getInitialItems_(): Array<OrganizerListSectionItem<unknown>> {
    return this.getFilteredItems_().slice(0, INITIAL_ITEM_COUNT);
  }

  protected getRemainingItems_(): Array<OrganizerListSectionItem<unknown>> {
    if (!this.expanded_) {
      return [];
    }
    return this.getFilteredItems_().slice(INITIAL_ITEM_COUNT);
  }

  protected hasMoreItems_(): boolean {
    return this.getFilteredItems_().length > INITIAL_ITEM_COUNT;
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  protected onItemClick_(e: Event) {
    const target = e.currentTarget as OrganizerListSectionItemElement;
    assert(target.item);
    this.delegate?.onItemClick(target.item);
  }

  protected getFilteredItems_(): Array<OrganizerListSectionItem<unknown>> {
    return this.searchQuery ? [] : this.items;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section': OrganizerListSectionElement;
  }
}

customElements.define(
    OrganizerListSectionElement.is, OrganizerListSectionElement);
