// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Base template with elements common to all Bluetooth UI sub-pages.
 */

import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';

import {assertNotReached} from '//resources/ash/common/assert.js';
import {focusWithoutInk} from '//resources/ash/common/focus_without_ink_js.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './bluetooth_base_page.html.js';
import {ButtonBarState, ButtonName, ButtonState} from './bluetooth_types.js';

/**
 * @constructor
 * @implements {I18nBehaviorInterface}
 * @extends {PolymerElement}
 */
const SettingsBluetoothBasePageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothBasePageElement extends
    SettingsBluetoothBasePageElementBase {
  static get is() {
    return 'bluetooth-base-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Object representing the states of each button in the button bar.
       * @type {!ButtonBarState}
       */
      buttonBarState: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      /** @type {boolean} */
      showScanProgress: {
        type: Boolean,
        value: false,
      },

      /**
       * Used to access |ButtonName| type in HTML.
       * @type {!ButtonName}
       */
      ButtonName: {
        type: Object,
        value: ButtonName,
      },

      /**
       * If true, sets the default focus to the first enabled button from the
       * end.
       * @type {boolean}
       */
      focusDefault: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    afterNextRender(this, () => {
      if (!this.focusDefault) {
        return;
      }

      const buttons = this.shadowRoot.querySelectorAll('cr-button');
      // Focus to the first non-disabled, from the end.
      for (let i = buttons.length - 1; i >= 0; i--) {
        const button = /** @type {!CrButtonElement}*/ (buttons.item(i));
        if (!button.disabled) {
          focusWithoutInk(button);
          return;
        }
      }
    });
  }

  /** @private */
  onCancelClick_() {
    this.dispatchEvent(new CustomEvent('cancel', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @private */
  onPairClick_() {
    this.dispatchEvent(new CustomEvent('pair', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * @param {!ButtonName} buttonName
   * @return {boolean}
   * @private
   */
  shouldShowButton_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state !== ButtonState.HIDDEN;
  }

  /**
   * @param {!ButtonName} buttonName
   * @return {boolean}
   * @private
   */
  isButtonDisabled_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state === ButtonState.DISABLED;
  }

  /**
   * @param {!ButtonName} buttonName
   * @return {!ButtonState}
   * @private
   */
  getButtonBarState_(buttonName) {
    switch (buttonName) {
      case ButtonName.CANCEL:
        return this.buttonBarState.cancel;
      case ButtonName.PAIR:
        return this.buttonBarState.pair;
      default:
        assertNotReached();
        return ButtonState.ENABLED;
    }
  }
}

customElements.define(
    SettingsBluetoothBasePageElement.is, SettingsBluetoothBasePageElement);
