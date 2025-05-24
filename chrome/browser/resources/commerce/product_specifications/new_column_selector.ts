// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './product_selection_menu.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './new_column_selector.css.js';
import {getHtml} from './new_column_selector.html.js';
import type {ProductSelectionMenuElement} from './product_selection_menu.js';

export interface NewColumnSelectorElement {
  $: {
    button: HTMLElement,
    productSelectionMenu: ProductSelectionMenuElement,
  };
}

export class NewColumnSelectorElement extends CrLitElement {
  static get is() {
    return 'new-column-selector';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      excludedUrls: {
        type: Array,
      },
      isTableFull: {
        type: Boolean,
      },
    };
  }

  accessor excludedUrls: string[] = [];
  accessor isTableFull: boolean = false;

  override render() {
    return getHtml.bind(this)();
  }

  closeMenu() {
    this.$.productSelectionMenu.close();
  }

  protected showMenu_() {
    this.$.productSelectionMenu.showAt(this.$.button);
    this.$.button.classList.add('showing-menu');
  }

  protected onCloseMenu_() {
    this.$.button.classList.remove('showing-menu');
  }

  protected onButtonKeyDown_(e: KeyboardEvent) {
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
