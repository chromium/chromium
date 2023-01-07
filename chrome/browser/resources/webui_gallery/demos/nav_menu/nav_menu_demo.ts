// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_drawer/cr_drawer.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import './nav_menu.js';

import {CrDrawerElement} from '//resources/cr_elements/cr_drawer/cr_drawer.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nav_menu_demo.html.js';

interface NavMenuDemoElement {
  $: {
    drawer: CrDrawerElement,
  };
}

class NavMenuDemoElement extends PolymerElement {
  static get is() {
    return 'nav-menu-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isDrawerOpen_: {
        type: Boolean,
        value: false,
      },

      selectedIndex_: Number,

      showIcons_: {
        type: Boolean,
        value: true,
      },

      showRipples_: {
        type: Boolean,
        value: true,
      },
    };
  }

  private isDrawerOpen_: boolean;
  private selectedIndex_: number;
  private showIcons_: boolean;
  private showRipples_: boolean;

  private showDrawerMenu_() {
    this.$.drawer.openDrawer();
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  private onDrawerClose_() {
    this.isDrawerOpen_ = this.$.drawer.open;
  }

  private onSelectedIndexChanged_(e: CustomEvent<{value: number}>) {
    this.selectedIndex_ = e.detail.value;
  }
}

customElements.define(NavMenuDemoElement.is, NavMenuDemoElement);
