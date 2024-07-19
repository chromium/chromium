// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './site_favicon.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './related_website_set_list_item.css.js';
import {getHtml} from './related_website_set_list_item.html.js';
import type {Member} from './related_website_sets.mojom-webui.js';
import {SiteType} from './related_website_sets.mojom-webui.js';

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
      managedByEnterprise: {type: Boolean},
      query: {type: String},
    };
  }

  expanded: boolean = false;
  primarySite: string = '';
  memberSites: Member[] = [];
  managedByEnterprise: boolean = false;
  query: string = '';

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded = e.detail.value;
    this.dispatchEvent(new CustomEvent(
        'expanded-toggled',
        {detail: {id: this.primarySite, expanded: this.expanded}}));
  }

  protected getSiteType_(type: SiteType): string {
    switch (type) {
      case SiteType.kPrimary:
        return 'Primary';
      case SiteType.kAssociated:
        return 'Associated';
      case SiteType.kService:
        return 'Service';
      default:
        throw new Error(`Unexpected site type ${type}`);
    }
  }

  protected isEnterpriseIconHidden_(): boolean {
    return !this.managedByEnterprise;
  }

  protected boldQuery_(site: string) {
    if (!this.query) {
      return site;
    }

    const queryLower = this.query.toLowerCase();
    const parts = site.split(new RegExp(`(${this.query})`, 'gi'));
    return parts.map(part =>
            part.toLowerCase() === queryLower ? html`<b>${part}</b>` : part);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'related-website-set-list-item': RelatedWebsiteSetListItemElement;
  }
}

customElements.define(
    RelatedWebsiteSetListItemElement.is, RelatedWebsiteSetListItemElement);
