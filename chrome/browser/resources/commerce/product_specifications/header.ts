// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './header_menu.js';

import {ProductSpecificationsBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/product_specifications_browser_proxy.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './header.css.js';
import {getHtml} from './header.html.js';
import type {HeaderMenuElement} from './header_menu.js';

export interface HeaderElement {
  $: {
    divider: HTMLElement,
    menuButton: CrIconButtonElement,
    menu: HeaderMenuElement,
  };
}

export class HeaderElement extends CrLitElement {
  static get is() {
    return 'product-specifications-header';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      // Whether the menu button and subtitle input are disabled.
      disabled: {
        type: Boolean,
        reflect: true,
      },

      isPageTitleClickable: {
        type: Boolean,
        reflect: true,
      },

      subtitle: {
        type: String,
        reflect: true,
      },

      maxNameLength_: {type: Number},

      showingMenu_: {
        type: Boolean,
        reflect: true,
      },

      showingInput_: {type: Boolean},
    };
  }

  accessor disabled: boolean = false;
  accessor isPageTitleClickable: boolean = false;
  accessor subtitle: string|null = null;

  protected accessor showingMenu_: boolean = false;
  protected accessor showingInput_: boolean = false;
  protected accessor maxNameLength_: number =
      loadTimeData.getInteger('maxNameLength');

  override render() {
    return getHtml.bind(this)();
  }

  protected showMenu_() {
    this.$.menu.showAt(this.$.menuButton);
    this.showingMenu_ = true;
  }

  protected onCloseMenu_() {
    this.showingMenu_ = false;
  }

  private get input_(): CrInputElement|null {
    const input = this.shadowRoot.querySelector('cr-input');
    return input;
  }

  protected async onRenaming_() {
    if (this.disabled) {
      return;
    }

    this.showingInput_ = true;
    await this.updateComplete;
    this.input_?.focus();
  }

  protected onInputKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      this.input_?.blur();
    }
  }

  protected onInputBlur_() {
    if (!this.input_) {
      return;
    }

    const inputValue = this.input_.value;
    this.showingInput_ = false;
    if (!inputValue) {
      if (this.subtitle) {
        this.input_.value = this.subtitle;
      }
    } else {
      this.subtitle = inputValue;
      this.dispatchEvent(new CustomEvent('name-change', {
        bubbles: true,
        composed: true,
        detail: {
          name: inputValue,
        },
      }));
    }

    // Move the cursor back to the end of the input.
    if (this.subtitle) {
      this.input_.select(this.subtitle.length, this.subtitle.length);
    }
  }

  protected onSubtitleKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      this.onRenaming_();
    }
  }

  protected onPageTitleClick_() {
    if (!this.isPageTitleClickable) {
      return;
    }

    ProductSpecificationsBrowserProxyImpl.getInstance().showComparePage(false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-header': HeaderElement;
  }
}

customElements.define(HeaderElement.is, HeaderElement);
