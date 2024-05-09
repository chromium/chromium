// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './header_menu.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './header.html.js';
import type {HeaderMenuElement} from './header_menu.js';

export interface HeaderElement {
  $: {
    menuButton: HTMLElement,
    menu: HeaderMenuElement,
  };
}

export class HeaderElement extends PolymerElement {
  static get is() {
    return 'product-specifications-header';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      subtitle: {
        type: String,
        reflectToAttribute: true,
      },

      showingMenu_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  subtitle: string|null = null;

  private showingMenu_: boolean;

  private showMenu_() {
    this.$.menu.showAt(this.$.menuButton);
    this.showingMenu_ = true;
  }

  private onCloseMenu_() {
    this.showingMenu_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-header': HeaderElement;
  }
}

customElements.define(HeaderElement.is, HeaderElement);
