// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import '//resources/cr_elements/cr_nav_menu_item_style.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrMenuSelector} from '//resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nav_menu.html.js';

interface NavMenuElement {
  $: {
    selector: CrMenuSelector,
  };
}

class NavMenuElement extends PolymerElement {
  static get is() {
    return 'nav-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      menuItems_: {
        type: Array,
        value: () => {
          return [
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
        },
      },
      selectedIndex: {
        type: Number,
        notify: true,
      },
      showIcons: {
        type: Boolean,
        value: false,
      },
      showRipples: {
        type: Boolean,
        value: false,
      },
    };
  }

  private menuItems_: Array<{icon: string, name: string, path: string}>;
  selectedIndex: number;
  showIcons: boolean;
  showRipples: boolean;

  private onSelectorClick_(e: MouseEvent) {
    e.preventDefault();
  }
}

customElements.define(NavMenuElement.is, NavMenuElement);
