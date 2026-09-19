// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import './organizer_list_section_item.js';

import type {CrUrlListItemElement} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section.css.js';
import {getHtml} from './organizer_list_section.html.js';
import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from './organizer_list_section_delegate.js';
import type {HighlightableOrganizerListSectionItem, OrganizerListSectionItem, OrganizerListSectionItemElement} from './organizer_list_section_item.js';
import type {SearchOptions} from './search_utils.js';
import {search} from './search_utils.js';

/**
 * This is the number of items in a section that are rendered before the "Show
 * more" option.
 */
export const INITIAL_ITEM_COUNT = 3;

export interface OrganizerListSectionElement {
  $: {
    header: HTMLElement,
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
      expanded_: {
        type: Boolean,
        reflect: true,
      },
      shouldRenderRemainingItems_: {type: Boolean},
      searchQuery: {type: String},
      filteredItems_: {type: Array},
      filteredSearchQuery_: {type: String},
    };
  }

  accessor delegate: OrganizerListSectionDelegate<unknown>|null = null;
  accessor items: Array<OrganizerListSectionItem<unknown>> = [];
  protected accessor expanded_: boolean = false;
  // True while overflow items should be populated in the DOM (from when
  // expansion starts until the collapse transition finishes).
  private accessor shouldRenderRemainingItems_: boolean = false;
  accessor searchQuery: string = '';
  protected accessor filteredItems_:
      Array<HighlightableOrganizerListSectionItem<unknown>> = [];
  // This is the search query that `filteredItems_` currently matches. This
  // ensures that we don't show the full list of elements until after the search
  // has been applied and the list of items has been filtered.
  protected accessor filteredSearchQuery_: string = '';

  private searchOptions_:
      SearchOptions<HighlightableOrganizerListSectionItem<unknown>> = {
        includeScore: true,
        includeMatches: true,
        ignoreLocation: false,
        threshold: 0.0,
        distance: 200,
        keys:
            [
              {
                name: 'title',
                getter: item => item.title,
                weight: 2,
              },
              {
                name: 'description',
                getter: item => item.description?.map(d => d.text),
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

    if (this.expanded_) {
      this.shouldRenderRemainingItems_ = true;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    this.updateExpandButtonAriaExpanded_();
  }

  /**
   * `<cr-url-list-item>` has no `aria-expanded` property, so set the attribute
   * directly on the focusable element it exposes. Awaiting the button's own
   * first update is required: this runs before a newly created child element
   * has rendered its shadow DOM, so its focusable element does not exist yet.
   */
  private async updateExpandButtonAriaExpanded_() {
    const expandButton =
        this.shadowRoot.querySelector<CrUrlListItemElement>('#expandButton');
    if (!expandButton) {
      return;
    }
    await expandButton.updateComplete;
    expandButton.getFocusableElement().setAttribute(
        'aria-expanded', this.expanded_ ? 'true' : 'false');
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
    if (query.length === 0) {
      this.filteredSearchQuery_ = '';
      this.filteredItems_ = [...this.items];
      return;
    }
    const filteredItems = await search(query, this.items, this.searchOptions_);
    // Confirm that the search query hasn't changed before updating the filtered
    // items.
    if (this.searchQuery === query) {
      this.filteredSearchQuery_ = query;
      this.filteredItems_ = filteredItems;
    }
  }

  private isSearching_(): boolean {
    return this.filteredSearchQuery_.length > 0;
  }

  protected getInitialItems_():
      Array<HighlightableOrganizerListSectionItem<unknown>> {
    if (this.isSearching_()) {
      return this.getFilteredItems_();
    }
    return this.getFilteredItems_().slice(0, INITIAL_ITEM_COUNT);
  }

  protected getRemainingItems_():
      Array<HighlightableOrganizerListSectionItem<unknown>> {
    if (!this.shouldRenderRemainingItems_ || this.isSearching_()) {
      return [];
    }
    return this.getFilteredItems_().slice(INITIAL_ITEM_COUNT);
  }

  protected hasMoreItems_(): boolean {
    return !this.isSearching_() &&
        this.getFilteredItems_().length > INITIAL_ITEM_COUNT;
  }

  protected hasNoSearchResults_(): boolean {
    return this.isSearching_() && this.getFilteredItems_().length === 0;
  }

  protected getExpandButtonLabel_(): string {
    return loadTimeData.getString(this.expanded_ ? 'showLess' : 'showMore');
  }

  protected getExpandButtonIcon_(): string {
    return this.expanded_ ? 'cr:keyboard-arrow-up' : 'cr:keyboard-arrow-down';
  }

  protected onExpandButtonClick_() {
    this.expanded_ = !this.expanded_;
  }

  protected onCollapseTransitionend_(e: TransitionEvent) {
    if (e.target === e.currentTarget && e.propertyName === 'height' &&
        !this.expanded_) {
      this.shouldRenderRemainingItems_ = false;
    }
  }

  protected onItemClick_(e: Event) {
    const target = e.currentTarget as OrganizerListSectionItemElement;
    assert(target.item);
    assert(this.delegate);
    this.delegate.onItemClick(target.item);
  }

  protected onItemActionButtonClick_(e: CustomEvent<{
    item: HighlightableOrganizerListSectionItem<unknown>,
    buttonElement: HTMLElement,
  }>) {
    assert(e.detail.item);
    assert(e.detail.buttonElement);
    assert(this.delegate);
    this.delegate.onItemActionButtonClicked?.(
        e.detail.item, e.detail.buttonElement);
  }

  protected getFilteredItems_():
      Array<HighlightableOrganizerListSectionItem<unknown>> {
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
