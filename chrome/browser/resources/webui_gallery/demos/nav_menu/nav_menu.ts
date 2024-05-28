// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/cr_elements/cr_ripple/cr_ripple.js';
import '//resources/cr_elements/icons_lit.html.js';

import type {CrMenuSelector} from '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {getCss} from '//resources/cr_elements/cr_nav_menu_item_style_lit.css.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './nav_menu.html.js';

export interface NavMenuElement {
  $: {
    selector: CrMenuSelector,
  };
}

export class NavMenuElement extends CrLitElement {
  static get is() {
    return 'nav-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      menuItems_: {type: Array},

      selectedIndex: {
        type: Number,
        notify: true,
      },

      showIcons: {type: Boolean},

      showRipples: {type: Boolean},
    };
  }

  protected menuItems_: Array<{icon: string, name: string, path: string}> = [
    {
      icon: 'cr:person',
      name: 'Menu item 1',
      path: '/path-1',
    },
    {
      icon: 'cr:sync',
      name: 'Menu item 2',
      path: '/path-2',
    },
    {
      icon: 'cr:star',
      name: 'Menu item 3',
      path: '/path-3',
    },
  ];
  selectedIndex?: number;
  showIcons: boolean = false;
  showRipples: boolean = false;

  protected onSelectorClick_(e: MouseEvent) {
    e.preventDefault();
  }

  protected onSelectedIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedIndex = e.detail.value;
  }
}

customElements.define(NavMenuElement.is, NavMenuElement);
