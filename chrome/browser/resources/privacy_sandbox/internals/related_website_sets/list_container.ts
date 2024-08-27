// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import './list_item.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {Member, RelatedWebsiteSet} from './related_website_sets.mojom-webui.js';
import {getCss} from './list_container.css.js';
import {getHtml} from './list_container.html.js';

export interface RelatedWebsiteSetsListContainerElement {
  $: {
    expandCollapseButton: CrButtonElement,
  };
}

export class RelatedWebsiteSetsListContainerElement extends CrLitElement {
  static get is() {
    return 'related-website-sets-list-container';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      relatedWebsiteSets: {type: Array},
      filteredItems: {type: Array},
      isAnyRowCollapsed: {type: Boolean},
      errorMessage: {type: String},
      query: {type: String},
    };
  }

  relatedWebsiteSets: RelatedWebsiteSet[] = [];
  query: string = '';
  errorMessage: string = '';
  protected isAnyRowCollapsed: boolean = true;
  filteredItems: RelatedWebsiteSet[] = [];

  private rowExpandedStates_: Map<string, boolean> = new Map();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    this.isAnyRowCollapsed = Array.from(this.rowExpandedStates_.values())
                                 .some(expanded => !expanded);

    if (changedProperties.has('query') ||
        changedProperties.has('relatedWebsiteSets')) {
      this.filteredItems =
          this.relatedWebsiteSets.filter(set => this.hasMatch_(set));

      this.errorMessage =
          this.filteredItems.length === 0 ? 'No items match' : '';
    }
  }

  private hasMatch_(set: RelatedWebsiteSet): boolean {
    const normalizedQuery = this.query.toLowerCase().trim();
    if (set.primarySite.toLowerCase().includes(normalizedQuery)) {
      return true;
    }

    for (const member of set.memberSites) {
      if (member.site.toLowerCase().includes(normalizedQuery)) {
        return true;
      }
    }
    return false;
  }

  protected onClick_() {
    const rows =
        this.shadowRoot!.querySelectorAll('related-website-sets-list-item');
    for (const row of rows) {
      row.expanded = this.isAnyRowCollapsed;
    }
  }

  protected onExpandedToggled_(e: CustomEvent<{
    id: string,
    expanded: boolean,
  }>) {
    this.rowExpandedStates_.set(e.detail.id, e.detail.expanded);
    this.requestUpdate();
  }

  protected expandCollapseButtonText_(): string {
    return this.isAnyRowCollapsed ? 'Expand All' : 'Collapse All';
  }

  protected getDisplayedError(): string {
    return this.errorMessage.replace('Error', '');
  }

  protected getMemberSites_(item: RelatedWebsiteSet): Member[] {
    return item.memberSites.filter(ms => ms.site !== item.primarySite);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'related-website-sets-list-container':
        RelatedWebsiteSetsListContainerElement;
  }
}

customElements.define(
    RelatedWebsiteSetsListContainerElement.is,
    RelatedWebsiteSetsListContainerElement);
