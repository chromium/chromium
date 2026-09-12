// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './organizer_list_section_item.js';

import {assert} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {SearchOptions} from '/tab_search/shared/search.js';
import {search} from '/tab_search/shared/search.js';

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
      filteredItems_: {type: Array},
    };
  }

  accessor delegate: OrganizerListSectionDelegate<unknown>|null = null;
  accessor items: Array<OrganizerListSectionItem<unknown>> = [];
  protected accessor expanded_: boolean = false;
  accessor searchQuery: string = '';
  protected accessor filteredItems_: Array<OrganizerListSectionItem<unknown>> =
      [];

  private searchOptions_: SearchOptions<OrganizerListSectionItem<unknown>> = {
    includeScore: true,
    includeMatches: true,
    ignoreLocation: false,
    threshold: 0.0,
    distance: 200,
    keys:
        [
          {
            name: 'title',
            getter: item => item.title.join(' '),
            weight: 2,
          },
          {
            name: 'description',
            getter: item => item.description?.map(d => d.text).join(' '),
            weight: 1,
          },
        ],
  };

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

    if (changedProperties.has('items') ||
        changedProperties.has('searchQuery')) {
      this.updateFilteredItems_();
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

  private async updateFilteredItems_() {
    const query = this.searchQuery;
    const filteredItems = await search(query, this.items, this.searchOptions_);
    // Confirm that the search query hasn't changed before updating the filtered
    // items.
    if (this.searchQuery === query) {
      this.filteredItems_ = filteredItems;
    }
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
    assert(this.delegate);
    this.delegate.onItemClick(target.item);
  }

  protected onItemActionButtonClick_(e: CustomEvent<{
    item: OrganizerListSectionItem<unknown>,
    buttonElement: HTMLElement,
  }>) {
    assert(e.detail.item);
    assert(e.detail.buttonElement);
    assert(this.delegate);
    this.delegate.onItemActionButtonClicked?.(
        e.detail.item, e.detail.buttonElement);
  }

  protected getFilteredItems_(): Array<OrganizerListSectionItem<unknown>> {
    return this.filteredItems_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section': OrganizerListSectionElement;
  }
}

customElements.define(
    OrganizerListSectionElement.is, OrganizerListSectionElement);
