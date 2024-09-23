// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_drawer/cr_drawer.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import './nav_menu.js';

import type {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './nav_menu_demo.css.js';
import {getHtml} from './nav_menu_demo.html.js';

export interface NavMenuDemoElement {
  $: {
    drawer: CrDrawerElement,
  };
}

export class NavMenuDemoElement extends CrLitElement {
  static get is() {
    return 'nav-menu-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isDrawerOpen_: {type: Boolean},
      selectedIndex_: {type: Number},
      showIcons_: {type: Boolean},
      showRipples_: {type: Boolean},
    };
  }

  protected isDrawerOpen_: boolean = false;
  protected selectedIndex_?: number;
  protected showIcons_: boolean = true;
  protected showRipples_: boolean = true;

  protected showDrawerMenu_() {
    this.$.drawer.openDrawer();
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  protected onDrawerClose_() {
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  protected onSelectedIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedIndex_ = e.detail.value;
  }

  protected onShowIconsChanged_(e: CustomEvent<{value: boolean}>) {
    this.showIcons_ = e.detail.value;
  }

  protected onShowRipplesChanged_(e: CustomEvent<{value: boolean}>) {
    this.showRipples_ = e.detail.value;
  }
}

export const tagName = NavMenuDemoElement.is;

customElements.define(NavMenuDemoElement.is, NavMenuDemoElement);
