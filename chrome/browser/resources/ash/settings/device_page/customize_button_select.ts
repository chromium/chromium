// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './input_device_settings_shared.css.js';
import './customize_button_dropdown_item.js';
import '../settings_shared.css.js';

import {DropdownMenuOptionList} from '/shared/settings/controls/settings_dropdown_menu.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOption} from './customize_button_dropdown_item.js';
import {getTemplate} from './customize_button_select.html.js';

/**
 * @fileoverview
 * 'customize-button-select' contains all the remapping actions for a
 * button. The user can click the component to display the dropdown menu
 * and select an action to customize the remapped button.
 */

export class CustomizeButtonSelectElement extends PolymerElement {
  static get is() {
    return 'customize-button-select' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      menu: {
        type: Object,
      },

      shouldShowDropdownMenu_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      selectedItem_: {
        type: Object,
      },
    };
  }

  menu: DropdownMenuOptionList;
  private shouldShowDropdownMenu_: boolean;
  private selectedItem_: DropdownMenuOption;

  override connectedCallback(): void {
    super.connectedCallback();
    this.addEventListener('blur', this.onBlur_);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.removeEventListener('blur', this.onBlur_);
  }

  private showDropdownMenu_(): void {
    this.shouldShowDropdownMenu_ = true;
  }

  private onBlur_(): void {
    this.shouldShowDropdownMenu_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CustomizeButtonSelectElement.is]: CustomizeButtonSelectElement;
  }
}

customElements.define(
    CustomizeButtonSelectElement.is, CustomizeButtonSelectElement);
