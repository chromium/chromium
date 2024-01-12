// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Element containing navigation buttons for the Cellular Setup flow. */
import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {assert, assertNotReached} from '//resources/ash/common/assert.js';
import {focusWithoutInk} from '//resources/ash/common/focus_without_ink_js.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './button_bar.html.js';
import {Button, ButtonBarState, ButtonState, CellularSetupPageName} from './cellular_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ButtonBarElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ButtonBarElement extends ButtonBarElementBase {
  static get is() {
    return 'button-bar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Sets the states of all buttons
       * @type {!ButtonBarState}
       */
      buttonState: {
        type: Object,
        value: {},
      },

      /**
       * @type {!Button}
       */
      Button: {
        type: Object,
        value: Button,
      },

      forwardButtonLabel: {
        type: String,
        value: '',
      },
    };
  }

  /**
   * @param {!Button} buttonName
   * @return {boolean}
   * @private
   */
  isButtonHidden_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state === ButtonState.HIDDEN;
  }

  /**
   * @param {!Button} buttonName
   * @return {boolean}
   * @private
   */
  isButtonDisabled_(buttonName) {
    const state = this.getButtonBarState_(buttonName);
    return state === ButtonState.DISABLED;
  }

  focusDefaultButton() {
    const buttons = this.shadowRoot.querySelectorAll('cr-button');
    // Focus the first non-disabled, non-hidden button from the end.
    for (let i = buttons.length - 1; i >= 0; i--) {
      const button = buttons.item(i);
      if (!button.disabled && !button.hidden) {
        focusWithoutInk(button);
        return;
      }
    }
  }

  /** @private */
  onBackwardButtonClicked_() {
    this.dispatchEvent(new CustomEvent('backward-nav-requested', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @private */
  onCancelButtonClicked_() {
    this.dispatchEvent(new CustomEvent('cancel-requested', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @private */
  onForwardButtonClicked_() {
    this.dispatchEvent(new CustomEvent('forward-nav-requested', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * @param {!Button} button
   * @returns {!ButtonState|undefined}
   * @private
   */
  getButtonBarState_(button) {
    assert(this.buttonState);
    switch (button) {
      case Button.BACKWARD:
        return this.buttonState.backward;
      case Button.CANCEL:
        return this.buttonState.cancel;
      case Button.FORWARD:
        return this.buttonState.forward;
      default:
        assertNotReached();
        return ButtonState.ENABLED;
    }
  }
}

customElements.define(ButtonBarElement.is, ButtonBarElement);
