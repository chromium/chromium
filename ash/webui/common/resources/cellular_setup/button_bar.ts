// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Element containing navigation buttons for the Cellular Setup flow. */
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './button_bar.html.js';
import {Button, ButtonBarState, ButtonState} from './cellular_types.js';

const ButtonBarElementBase = I18nMixin(PolymerElement);

export class ButtonBarElement extends ButtonBarElementBase {
  static get is() {
    return 'button-bar' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Sets the states of all buttons
       */
      buttonState: {
        type: Object,
        value: {},
      },

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

  buttonState: ButtonBarState;
  forwardButtonLabel: string|undefined;

  private isButtonHidden_(buttonName: Button): boolean {
    const state = this.getButtonBarState_(buttonName);
    return state === ButtonState.HIDDEN;
  }

  private isButtonDisabled_(buttonName: Button): boolean {
    const state = this.getButtonBarState_(buttonName);
    return state === ButtonState.DISABLED;
  }

  focusDefaultButton(): void {
    const buttons = this.shadowRoot!.querySelectorAll('cr-button');
    // Focus the first non-disabled, non-hidden button from the end.
    for (let i = buttons.length - 1; i >= 0; i--) {
      const button = buttons.item(i);
      if (!button.disabled && !button.hidden) {
        focusWithoutInk(button);
        return;
      }
    }
  }

  private onCancelButtonClicked_(): void {
    this.dispatchEvent(new CustomEvent('cancel-requested', {
      bubbles: true,
      composed: true,
    }));
  }

  private onForwardButtonClicked_(): void {
    this.dispatchEvent(new CustomEvent('forward-nav-requested', {
      bubbles: true,
      composed: true,
    }));
  }

  private getButtonBarState_(button: Button): ButtonState|undefined {
    assert(this.buttonState);
    switch (button) {
      case Button.CANCEL:
        return this.buttonState.cancel;
      case Button.FORWARD:
        return this.buttonState.forward;
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ButtonBarElement.is]: ButtonBarElement;
  }
}

customElements.define(ButtonBarElement.is, ButtonBarElement);
