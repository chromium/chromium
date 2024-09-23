// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_drawer/cr_drawer.js';
import './sidebar.js';
import './toolbar.js';
import './list_container.js';

import type {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {RelatedWebsiteSet} from './related_website_sets.mojom-webui.js';
import type {RelatedWebsiteSetsApiBrowserProxy} from './related_website_sets_api_proxy.js';
import {RelatedWebsiteSetsApiBrowserProxyImpl} from './related_website_sets_api_proxy.js';
import type {RelatedWebsiteSetsSidebarElement} from './sidebar.js';
import type {RelatedWebsiteSetsToolbarElement} from './toolbar.js';

export interface RelatedWebsiteSetsAppElement {
  $: {
    drawer: CrDrawerElement,
    toolbar: RelatedWebsiteSetsToolbarElement,
    sidebar: RelatedWebsiteSetsSidebarElement,
  };
}

export class RelatedWebsiteSetsAppElement extends CrLitElement {
  static get is() {
    return 'related-website-sets-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageTitle_: {type: String},
      narrow_: {type: Boolean},
      isDrawerOpen_: {type: Boolean},
      relatedWebsiteSets_: {type: Array},
      errorMessage_: {type: String},
      query_: {type: String},
    };
  }

  // TODO (b/330877132): Add Localization once UI stable
  protected pageTitle_: string = 'Related Website Sets';
  protected narrow_: boolean = true;
  protected isDrawerOpen_: boolean = false;
  protected relatedWebsiteSets_: RelatedWebsiteSet[] = [];
  protected errorMessage_: string = '';
  protected query_: string = '';

  private apiProxy_: RelatedWebsiteSetsApiBrowserProxy =
      RelatedWebsiteSetsApiBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.updateQueryFromUrl_();
    window.addEventListener(
        'popstate', (() => this.handlePopState_()) as EventListener);

    this.apiProxy_.handler.getRelatedWebsiteSets().then(
        ({relatedWebsiteSetsInfo}) => {
          if (relatedWebsiteSetsInfo.errorMessage) {
            this.errorMessage_ = relatedWebsiteSetsInfo.errorMessage;
          } else if (relatedWebsiteSetsInfo.relatedWebsiteSets) {
            this.relatedWebsiteSets_ =
                relatedWebsiteSetsInfo.relatedWebsiteSets;
          }
        });
  }

  override disconnectedCallback() {
    window.removeEventListener(
      'popstate', (() => this.handlePopState_()) as EventListener);
    super.disconnectedCallback();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('narrow_')) {
      if (this.$.drawer.open && !this.narrow_) {
        this.$.drawer.close();
      }
    }
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }

  protected onMenuButtonClick_() {
    this.showDrawerMenu_();
  }

  private showDrawerMenu_() {
    this.$.drawer.openDrawer();
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  protected onDrawerClose_() {
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  protected onSearchChanged_(event: CustomEvent<string>) {
    this.query_ = event.detail;
    this.updateUrlParams_();
  }

  private updateUrlParams_() {
    const urlParams = new URLSearchParams(window.location.search);
    if (this.query_) {
      urlParams.set('q', this.query_);
    } else {
      urlParams.delete('q');
    }
    window.history.replaceState(
        {}, '', `${window.location.pathname}?${urlParams.toString()}`);
  }

  private updateQueryFromUrl_() {
    const params = new URLSearchParams(window.location.search);
    this.query_ = params.get('q') || '';
    this.$.toolbar.setSearchFieldValue(this.query_);
  }

  private handlePopState_() {
    this.updateQueryFromUrl_();
  }

  setNarrowForTesting(state: boolean) {
    this.narrow_ = state;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'related-website-sets-app': RelatedWebsiteSetsAppElement;
  }
}

customElements.define(
    RelatedWebsiteSetsAppElement.is, RelatedWebsiteSetsAppElement);
