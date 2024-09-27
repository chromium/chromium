// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './product_selection_menu.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './new_column_selector.html.js';
import type {ProductSelectionMenuElement} from './product_selection_menu.js';

export interface NewColumnSelectorElement {
  $: {
    button: HTMLElement,
    productSelectionMenu: ProductSelectionMenuElement,
  };
}

export class NewColumnSelectorElement extends PolymerElement {
  static get is() {
    return 'new-column-selector';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      excludedUrls: {
        type: Array,
        value: () => [],
      },
      isTableFull: {
        type: Boolean,
        value: false,
      },
    };
  }

  excludedUrls: string[];
  isTableFull: boolean;

  private showMenu_() {
    this.$.productSelectionMenu.showAt(this.$.button);
    this.$.button.classList.add('showing-menu');
  }

  private onCloseMenu_() {
    this.$.button.classList.remove('showing-menu');
  }

  private onButtonKeyDown_(e: KeyboardEvent) {
    if (e.key === ' ' || e.key === 'Enter') {
      e.preventDefault();
      e.stopPropagation();
      this.showMenu_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'new-column-selector': NewColumnSelectorElement;
  }
}

customElements.define(NewColumnSelectorElement.is, NewColumnSelectorElement);
