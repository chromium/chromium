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
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './list_item.css.js';
import {getHtml} from './list_item.html.js';
import type {Member} from './related_website_sets.mojom-webui.js';
import {SiteType} from './related_website_sets.mojom-webui.js';

export interface RelatedWebsiteSetsListItemElement {
  $: {
    expandedContent: CrCollapseElement,
    expandButton: CrExpandButtonElement,
  };
}

export class RelatedWebsiteSetsListItemElement extends CrLitElement {
  static get is() {
    return 'related-website-sets-list-item';
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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('expanded')) {
      this.dispatchEvent(new CustomEvent(
          'expanded-toggled',
          {detail: {id: this.primarySite, expanded: this.expanded}}));
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('query')) {
      if (this.query) {
        const memberSitesWithMatch = this.memberSites.filter(
            member =>
                member.site.toLowerCase().includes(this.query.toLowerCase()));
        this.expanded = memberSitesWithMatch.length > 0;
      } else {
        this.expanded = false;
      }
    }
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded = e.detail.value;
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

  protected boldQuery_(url: string) {
    const domain = url.includes('://') ? url.split('://')[1]! : url;
    if (!this.query) {
      return domain;
    }

    const queryLower = this.query.toLowerCase();
    const parts = domain.split(new RegExp(`(${this.query})`, 'gi'));

    return parts.map(part =>
            part.toLowerCase() === queryLower ? html`<b>${part}</b>` : part);
  }

  protected getIconImageUrl_(site: string): string {
    return `${site}/favicon.ico`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'related-website-sets-list-item': RelatedWebsiteSetsListItemElement;
  }
}

customElements.define(
    RelatedWebsiteSetsListItemElement.is, RelatedWebsiteSetsListItemElement);
