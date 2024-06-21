// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-menu-item' represents a menu item. This is expected to be
 * rendered under a 'iron-selector' element in 'os-settings-menu'.
 */

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
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

      label: {
        type: String,
        value: '',
      },

      sublabel: {
        type: String,
        value: '',
      },

      /**
       * Mirrors `label` to the `aria-label` attribute and is used solely for
       * a11y purposes.
       */
      ariaLabel: {
        type: String,
        reflectToAttribute: true,
        computed: 'computeAriaLabel_(label)',
      },

      /**
       * Mirrors `sublabel` to the `aria-description` attribute and is used
       * solely for a11y purposes.
       */
      ariaDescription: {
        type: String,
        reflectToAttribute: true,
        computed: 'computeAriaDescription_(sublabel)',
      },

      tooltipPosition: {
        type: String,
        value: 'right',
      },
    };
  }

  icon: string;
  path: string;
  label: string;
  sublabel: string;
  override ariaDescription: string|null;
  tooltipPosition: 'right'|'left'|'bottom';

  override ready(): void {
    super.ready();

    this.setAttribute('role', 'link');
    this.setAttribute('tabindex', '0');
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  /**
   * Mirrors `label` to `ariaLabel` for a11y purposes.
   */
  private computeAriaLabel_(): string {
    return this.label;
  }

  /**
   * Mirrors `sublabel` to `ariaDescription` for a11y purposes.
   */
  private computeAriaDescription_(): string {
    return this.sublabel;
  }

  /**
   * Simulate a click only when the Enter or Space key is pressed.
   */
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
    this.dispatchEvent(
        new CustomEvent('click', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsMenuItemElement.is]: OsSettingsMenuItemElement;
  }
}

customElements.define(OsSettingsMenuItemElement.is, OsSettingsMenuItemElement);
