// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './header_menu.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './header.html.js';
import type {HeaderMenuElement} from './header_menu.js';

export interface HeaderElement {
  $: {
    divider: HTMLElement,
    menuButton: CrIconButtonElement,
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
      menuButtonDisabled: {
        type: Boolean,
        value: false,
      },

      subtitle: {
        type: String,
        reflectToAttribute: true,
      },

      showingMenu_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      showingInput_: {
        type: Boolean,
        value: false,
      },
    };
  }

  menuButtonDisabled: boolean;
  subtitle: string|null = null;

  private showingMenu_: boolean;
  private showingInput_: boolean;
  private pageName_: string;
  private maxNameLength_: number = loadTimeData.getInteger('maxNameLength');

  private showMenu_() {
    this.$.menu.showAt(this.$.menuButton);
    this.showingMenu_ = true;
  }

  private onCloseMenu_() {
    this.showingMenu_ = false;
  }

  private get input_(): CrInputElement {
    const input = this.shadowRoot!.querySelector('cr-input');
    assert(!!input);
    return input;
  }

  private onRenaming_() {
    this.showingInput_ = true;
    afterNextRender(this, () => this.input_.focus());
  }

  private onInputKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      this.input_.blur();
    }
  }

  private onInputBlur_() {
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

  private onSubtitleKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      this.onRenaming_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-header': HeaderElement;
  }
}

customElements.define(HeaderElement.is, HeaderElement);
