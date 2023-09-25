// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-menu-item' represents a menu item. This is expected to be
 * rendered under a 'iron-selector' element in 'os-settings-menu'.
 */

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_settings_icons.html.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './menu_item.html.js';

export class OsSettingsMenuItemElement extends PolymerElement {
  static get is() {
    return 'os-settings-menu-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path: {
        type: String,
        reflectToAttribute: true,
      },

      /**
       * Icon type must be registered in os_settings_icons.html. By default,
       * no icon is shown.
       */
      icon: {
        type: String,
        value: '',
      },

      sublabel: {
        type: String,
        value: '',
      },
    };
  }

  icon: string;
  path: string;
  sublabel: string;

  override ready(): void {
    super.ready();

    this.setAttribute('role', 'link');
    this.setAttribute('tabindex', '0');
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  private onKeyDown_(event: KeyboardEvent): void {
    if (event.key !== ' ' && event.key !== 'Enter') {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    if (event.repeat) {
      return;
    }

    // Simulate click
    if (event.key === 'Enter') {
      this.dispatchEvent(
          new CustomEvent('click', {bubbles: true, composed: true}));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsMenuItemElement.is]: OsSettingsMenuItemElement;
  }
}

customElements.define(OsSettingsMenuItemElement.is, OsSettingsMenuItemElement);
