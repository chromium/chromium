// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar.js';
import '//resources/cr_elements/cr_drawer/cr_drawer.js';
import './sidebar.js';
import './container.js';

import type {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {PrivateStateTokensSidebarElement} from './sidebar.js';
import type {PrivateStateTokensToolbarElement} from './toolbar.js';


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
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener(
        'cr-toolbar-menu-click', this.onMenuButtonClick_ as EventListener);
    this.addEventListener(
        'narrow-changed', this.onNarrowChanged_ as EventListener);
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
