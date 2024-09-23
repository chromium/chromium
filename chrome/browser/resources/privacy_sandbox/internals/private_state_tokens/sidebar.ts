// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_ripple/cr_ripple.js';
import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';

import type {CrMenuSelector} from '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './sidebar.css.js';
import {getHtml} from './sidebar.html.js';

interface MenuItem {
  icon: string;
  name: string;
}

export interface PrivateStateTokensSidebarElement {
  $: {
    selector: CrMenuSelector,
  };
}
export class PrivateStateTokensSidebarElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-sidebar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      menuItems: {type: Array},
    };
  }

  protected menuItems: Array<{icon: string, name: string, path: string}> = [
    {
      icon: 'cr:settings_icon',
      name: 'Settings',
      path: 'chrome://settings',
    },
  ];

  getMenuItemsForTesting(): MenuItem[] {
    return this.menuItems;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-sidebar': PrivateStateTokensSidebarElement;
  }
}

customElements.define(
    PrivateStateTokensSidebarElement.is, PrivateStateTokensSidebarElement);
