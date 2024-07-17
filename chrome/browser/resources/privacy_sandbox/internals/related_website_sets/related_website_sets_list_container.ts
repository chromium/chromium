// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import './related_website_set_list_item.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {RelatedWebsiteSetListItemElement} from 'related_website_set_list_item.js';

import type {RelatedWebsiteSet} from './related_website_sets.mojom-webui.js';
import {getCss} from './related_website_sets_list_container.css.js';
import {getHtml} from './related_website_sets_list_container.html.js';

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
      isAnyRowCollapsed: {type: Boolean},
    };
  }

  protected relatedWebsiteSets: RelatedWebsiteSet[] = [];
  protected isAnyRowCollapsed: boolean = true;

  private rowExpandedStates_: Map<string, boolean> = new Map();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    this.isAnyRowCollapsed = Array.from(this.rowExpandedStates_.values())
                                 .some(expanded => !expanded);
  }

  protected onClick_() {
    const rows =
        this.shadowRoot!.querySelectorAll('related-website-set-list-item');
    rows.forEach(
        (row: RelatedWebsiteSetListItemElement) =>
            row.setExpanded(this.isAnyRowCollapsed));
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

  setRelatedWebsiteSetsForTesting(sets: RelatedWebsiteSet[]) {
    this.relatedWebsiteSets = sets;
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
