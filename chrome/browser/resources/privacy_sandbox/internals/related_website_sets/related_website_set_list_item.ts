// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './site_favicon.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './related_website_set_list_item.css.js';
import {getHtml} from './related_website_set_list_item.html.js';
import type {Member, RelatedWebsiteSet} from './related_website_sets.mojom-webui.js';

export interface RelatedWebsiteSetListItemElement {
  $: {
    expandedContent: CrCollapseElement,
    expandButton: CrExpandButtonElement,
  };
}

export class RelatedWebsiteSetListItemElement extends CrLitElement {
  static get is() {
    return 'related-website-set-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      expanded: {type: Boolean},
      primarySite: {type: String},
      memberSites: {type: Array},
    };
  }

  protected expanded: boolean = false;
  protected primarySite: string = '';
  protected memberSites: Member[] = [];

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded = e.detail.value;
    this.dispatchEvent(new CustomEvent(
        'expanded-toggled',
        {detail: {id: this.primarySite, expanded: this.expanded}}));
  }

  setExpanded(value: boolean) {
    this.expanded = value;
  }

  setItemForTesting(set: RelatedWebsiteSet) {
    this.primarySite = set.primarySite;
    this.memberSites = set.memberSites;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'related-website-set-list-item': RelatedWebsiteSetListItemElement;
  }
}

customElements.define(
    RelatedWebsiteSetListItemElement.is, RelatedWebsiteSetListItemElement);
