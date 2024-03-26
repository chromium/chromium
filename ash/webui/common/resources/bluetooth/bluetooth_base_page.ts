// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Base template with elements common to all Bluetooth UI sub-pages.
 */

import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_base_page.html.js';
import {ButtonBarState, ButtonName, ButtonState} from './bluetooth_types.js';

const SettingsBluetoothBasePageElementBase = I18nMixin(PolymerElement);

export class SettingsBluetoothBasePageElement extends
    SettingsBluetoothBasePageElementBase {
  static get is() {
    return 'bluetooth-base-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Object representing the states of each button in the button bar.
       */
      buttonBarState: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      showScanProgress: {
        type: Boolean,
        value: false,
      },

      /**
       * Used to access |ButtonName| type in HTML.
       */
      ButtonName: {
        type: Object,
        value: ButtonName,
      },

      /**
       * If true, sets the default focus to the first enabled button from the
       * end.
       */
      focusDefault: {
        type: Boolean,
        value: false,
      },
    };
  }

  buttonBarState: ButtonBarState;
  showScanProgress: boolean;
  focusDefault: boolean;

  override connectedCallback(): void {
    super.connectedCallback();
    afterNextRender(this, () => this.focus());
  }

  override focus(): void {
    super.focus();
    if (!this.focusDefault) {
      return;
    }

    const buttons = this.shadowRoot!.querySelectorAll('cr-button');
    // Focus to the first non-disabled, from the end.
    for (let i = buttons.length - 1; i >= 0; i--) {
      const button = buttons.item(i);
      if (!button.disabled) {
        focusWithoutInk(button);
        return;
      }
    }
  }

  private onCancelClick_(): void {
    this.dispatchEvent(new CustomEvent('cancel', {
      bubbles: true,
      composed: true,
    }));
  }

  private onPairClick_(): void {
    this.dispatchEvent(new CustomEvent('pair', {
      bubbles: true,
      composed: true,
    }));
  }

  private shouldShowButton_(buttonName: ButtonName): boolean {
    const state = this.getButtonBarState_(buttonName);
    return state !== ButtonState.HIDDEN;
  }

  private isButtonDisabled_(buttonName: ButtonName): boolean {
    const state = this.getButtonBarState_(buttonName);
    return state === ButtonState.DISABLED;
  }

  private  getButtonBarState_(buttonName: ButtonName): ButtonState {
    switch (buttonName) {
      case ButtonName.CANCEL:
        return this.buttonBarState.cancel;
      case ButtonName.PAIR:
        return this.buttonBarState.pair;
      default:
        return ButtonState.ENABLED;
    }
  }
}

declare global {
    interface HTMLElementTagNameMap {
      [SettingsBluetoothBasePageElement.is]: SettingsBluetoothBasePageElement;
    }
}

customElements.define(
    SettingsBluetoothBasePageElement.is, SettingsBluetoothBasePageElement);
