// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import './organizer_list_section_header.js';
import './organizer_list_section_item.js';

import {assert} from '//resources/js/assert.js';
import type {PropertyValues, TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_list_section.css.js';
import {getHtml} from './organizer_list_section.html.js';
import type {OrganizerListSectionClient, OrganizerListSectionDelegate} from './organizer_list_section_delegate.js';
import type {OrganizerListSectionHeaderElement} from './organizer_list_section_header.js';
import type {HighlightableOrganizerListSectionItem, OrganizerListSectionItem, OrganizerListSectionItemElement} from './organizer_list_section_item.js';
import type {BrowserProxy, SectionState} from './organizer_panel.mojom-webui.js';
import {browserProxyFactory} from './organizer_panel.mojom-webui.js';
import type {SearchOptions} from './search_utils.js';
import {search} from './search_utils.js';

export const INITIAL_ITEM_COUNT = 3;

export interface OrganizerListSectionElement {
  $: {
    header: OrganizerListSectionHeaderElement,
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
      noAnimation_: {type: Boolean},
      showAll_: {type: Boolean},
      searchQuery: {type: String},
      filteredItems_: {type: Array},
      filteredSearchQuery_: {type: String},
      hidden: {
        type: Boolean,
        reflect: true,
        notify: true,
      },
    };
  }

  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();
  private sectionStateChangedListenerId_: number|null = null;
  accessor delegate: OrganizerListSectionDelegate<unknown>|null = null;
  accessor items: Array<OrganizerListSectionItem<unknown>> = [];
  protected accessor expanded_: boolean = true;
  protected accessor noAnimation_: boolean = false;
  protected accessor showAll_: boolean = false;
  accessor searchQuery: string = '';
  protected accessor filteredItems_:
      Array<HighlightableOrganizerListSectionItem<unknown>> = [];
  // This is the search query that `filteredItems_` currently matches. This
  // ensures that we don't show the full list of elements until after the search
  // has been applied and the list of items has been filtered.
  protected accessor filteredSearchQuery_: string = '';
  override accessor hidden: boolean = false;

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
      this.updateSectionState_();
    }
  };

  override connectedCallback() {
    super.connectedCallback();
    document.addEventListener('visibilitychange', this.onVisibilityChange_);
    this.sectionStateChangedListenerId_ =
        this.browserProxy_.callbackRouter.onSectionsStateChanged.addListener(
            (sectionsState: Record<string, SectionState>) => {
              this.onSectionsStateChanged_(sectionsState);
            });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
    assert(this.sectionStateChangedListenerId_ !== null);
    this.browserProxy_.callbackRouter.removeListener(
        this.sectionStateChangedListenerId_);
    this.sectionStateChangedListenerId_ = null;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('delegate')) {
      this.delegate?.init(this);
      this.updateItems_();
      this.updateSectionState_();
    }

    if (changedProperties.has('items') ||
        changedProperties.has('searchQuery')) {
      this.updateFilteredItems_();
    }

    this.hidden = this.hasNoSearchResults_();
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

  private async updateSectionState_() {
    if (!this.delegate) {
      return;
    }
    const delegate = this.delegate;
    const {state} =
        await this.browserProxy_.handler.getSectionState(delegate.getId());
    if (this.delegate !== delegate) {
      return;
    }
    // Disable the transition when the expanded state is loaded from prefs.
    this.noAnimation_ = true;
    this.expanded_ = state.expanded;
    this.showAll_ = state.showAll;
    await this.updateComplete;
    this.noAnimation_ = false;
  }

  private onSectionsStateChanged_(sectionsState: Record<string, SectionState>) {
    assert(this.delegate);
    const state = sectionsState[this.delegate.getId()];
    assert(state);
    this.expanded_ = state.expanded;
    this.showAll_ = state.showAll;
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

  protected isSearching_(): boolean {
    return this.filteredSearchQuery_.length > 0;
  }

  protected isExpanded_(): boolean {
    return this.expanded_ || this.isSearching_();
  }

  private hasNoSearchResults_(): boolean {
    return this.isSearching_() && this.getFilteredItems_().length === 0;
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    if (this.isSearching_() || this.expanded_ === e.detail.value) {
      return;
    }
    this.expanded_ = e.detail.value;
    assert(this.delegate);
    this.browserProxy_.handler.setSectionExpanded(
        this.delegate.getId(), this.expanded_);
  }

  protected hasZeroState_(): boolean {
    return !this.isSearching_() &&
        (this.getFilteredItems_().length === 0 ||
         !!this.delegate?.shouldAlwaysShowZeroState?.()) &&
        !!this.getZeroState_();
  }

  protected getZeroState_(): TemplateResult|undefined {
    return this.delegate?.getZeroState?.();
  }

  protected onShowAllChanged_(e: CustomEvent<{value: boolean}>) {
    if (this.showAll_ === e.detail.value) {
      return;
    }
    this.showAll_ = e.detail.value;
    assert(this.delegate);
    this.browserProxy_.handler.setSectionShowAll(
        this.delegate.getId(), this.showAll_);
  }

  protected onItemClick_(e: Event) {
    const target = e.currentTarget as OrganizerListSectionItemElement;
    assert(target.item);
    assert(this.delegate);
    this.delegate.onItemClick(target.item);
    this.browserProxy_.handler.closePanel();
  }

  protected async onItemActionButtonClick_(e: CustomEvent<{
    item: HighlightableOrganizerListSectionItem<unknown>,
    buttonElement: HTMLElement,
  }>) {
    assert(e.detail.item);
    assert(e.detail.buttonElement);
    assert(this.delegate);
    const itemElement = e.currentTarget as OrganizerListSectionItemElement;
    await this.delegate.onItemActionButtonClicked?.(
        e.detail.item, e.detail.buttonElement);
    itemElement.resetActionButtonStateIfNeeded();
  }

  protected async onItemContextMenuClick_(e: CustomEvent<{
    item: HighlightableOrganizerListSectionItem<unknown>,
    x: number,
    y: number,
  }>) {
    assert(e.detail.item);
    assert(this.delegate);
    await this.delegate.onItemContextMenuClicked?.(
        e.detail.item, e.detail.x, e.detail.y);
  }

  protected getFilteredItems_():
      Array<HighlightableOrganizerListSectionItem<unknown>> {
    if (this.showAll_ || this.isSearching_()) {
      return this.filteredItems_;
    }
    return this.filteredItems_.slice(0, INITIAL_ITEM_COUNT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-list-section': OrganizerListSectionElement;
  }
}

customElements.define(
    OrganizerListSectionElement.is, OrganizerListSectionElement);
