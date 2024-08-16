// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar.js';
import '//resources/cr_elements/cr_drawer/cr_drawer.js';
import './sidebar.js';
import './navigation.js';

import type {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {PrivateStateTokensApiBrowserProxy} from './browser_proxy.js';
import {PrivateStateTokensApiBrowserProxyImpl} from './browser_proxy.js';
import type {IssuerTokenCount} from './private_state_tokens.js';
import type {PrivateStateTokensSidebarElement} from './sidebar.js';
import type {PrivateStateTokensToolbarElement} from './toolbar.js';
import {ItemsToRender, nullMetadataObj} from './types.js';
import type {ListItem, Metadata} from './types.js';

export interface PrivateStateTokensAppElement {
  $: {
    drawer: CrDrawerElement,
    sidebar: PrivateStateTokensSidebarElement,
    toolbar: PrivateStateTokensToolbarElement,
  };
}

export class PrivateStateTokensAppElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-app';
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
      narrowThreshold_: {type: Number},
      isDrawerOpen_: {type: Boolean},
      itemToRender: {type: String},
      data: {type: Array},
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.updateIssuerTokenCounts_();
    this.addEventListener(
        'cr-toolbar-menu-click', this.onMenuButtonClick_ as EventListener);
    this.addEventListener(
        'narrow-changed', this.onNarrowChanged_ as EventListener);
  }

  override firstUpdated() {
    window.addEventListener(
        'navigate-to-metadata',
        this.handleNavigationToMetadata_ as EventListener);
    window.addEventListener(
        'navigate-to-container', this.handleNavigationToList_ as EventListener);
  }

  override disconnectedCallback() {
    this.removeEventListener(
        'cr-toolbar-menu-click', this.onMenuButtonClick_ as EventListener);
    this.removeEventListener(
        'narrow-changed', this.onNarrowChanged_ as EventListener);
    super.disconnectedCallback();
  }

  constructor() {
    super();
    this.narrow_ = window.innerWidth < this.narrowThreshold_;
  }

  protected narrowThreshold_: number = 1096;
  protected narrow_: boolean;
  protected pageTitle_: string = 'Private State Tokens';
  protected isDrawerOpen_: boolean = false;
  itemToRender: ItemsToRender = ItemsToRender.ISSUER_LIST;
  protected metadata_?: Metadata;
  private handleNavigationToMetadata_ =
      this.handleContentNavigation_.bind(this, ItemsToRender.ISSUER_METADATA);
  private handleNavigationToList_ =
      this.handleContentNavigation_.bind(this, ItemsToRender.ISSUER_LIST);
  data: ListItem[] = [];

  private browserProxy: PrivateStateTokensApiBrowserProxy =
      PrivateStateTokensApiBrowserProxyImpl.getInstance();

  private async updateIssuerTokenCounts_() {
    const newIssuer = await this.browserProxy.handler.getIssuerTokenCounts();
    this.data = newIssuer.privateStateTokensCount
                    .sort(
                        (a: IssuerTokenCount, b: IssuerTokenCount) =>
                            a.issuer.localeCompare(b.issuer))
                    .map(
                        issuerObj => ({
                          issuerOrigin: issuerObj.issuer,
                          numTokens: issuerObj.count,
                          // TODO(crbug.com/348590926): redemptions data will
                          // be plumbed in a following cl
                          redemptions: [],
                          metadata: nullMetadataObj,
                        }),
                    );
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }
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

  protected handleContentNavigation_(
      itemToRender: ItemsToRender, event: CustomEvent<Metadata>) {
    this.itemToRender = itemToRender;
    if (this.itemToRender === ItemsToRender.ISSUER_METADATA) {
      this.metadata_ = event.detail;
    }
    this.requestUpdate();
  }

  setNarrowForTesting(state: boolean) {
    this.narrow_ = state;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-app': PrivateStateTokensAppElement;
  }
}

customElements.define(PrivateStateTokensAppElement.is, PrivateStateTokensAppElement);
