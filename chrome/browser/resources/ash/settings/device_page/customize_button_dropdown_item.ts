// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import './input_device_settings_shared.css.js';
import '../settings_shared.css.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_button_dropdown_item.html.js';

/**
 * @fileoverview
 * 'customize-button-dropdown-item' contains the option item for the user to
 * customize the remapped button.
 */

export type DropdownItemSelectEvent = CustomEvent<{value: number | string}>;

export interface DropdownMenuOption {
  name: string;
  value: number|string;
  hidden?: boolean;
}

export interface CustomizeButtonDropdownItemElement {
  $: {
    container: HTMLDivElement,
  };
}

export class CustomizeButtonDropdownItemElement extends PolymerElement {
  static get is() {
    return 'customize-button-dropdown-item' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      selected: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      option: {
        type: Object,
      },
    };
  }

  selected: boolean;
  option: DropdownMenuOption;

  override focus(): void {
    super.focus();

    this.$.container.focus();
  }

  private onDropdownItemSelected_(): void {
    this.dispatchEvent(new CustomEvent('customize-button-dropdown-selected', {
      bubbles: true,
      composed: true,
      detail: {
        value: this.option.value,
      },
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonDropdownItemElement.is]: CustomizeButtonDropdownItemElement;
  }
}

customElements.define(
    CustomizeButtonDropdownItemElement.is, CustomizeButtonDropdownItemElement);
