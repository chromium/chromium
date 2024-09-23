// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';

import {CrRadioButtonMixin} from 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button_mixin.js';
import {PaperRippleMixin} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {PaperRippleElement} from 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './multidevice_radio_button.html.js';

const MultideviceRadioButtonElementBase =
    PaperRippleMixin(CrRadioButtonMixin(PolymerElement));

class MultideviceRadioButtonElement extends MultideviceRadioButtonElementBase {
  static get is() {
    return 'multidevice-radio-button' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ariaChecked: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getAriaChecked_(checked)',
      },
      ariaDisabled: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getAriaDisabled_(disabled)',

      },
      ariaLabel: {
        type: String,
        notify: true,
        reflectToAttribute: true,
        computed: 'getLabel_(label)',
      },
    };
  }

  override ready(): void {
    super.ready();
    this.setAttribute('role', 'radio');
  }

  // Overridden from CrRadioButtonMixin
  override getPaperRipple(): PaperRippleElement {
    return this.getRipple();
  }

  // Overridden from PaperRippleMixin
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple(): PaperRippleElement {
    this._rippleContainer = this.shadowRoot!.querySelector('.disc-wrapper');
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }

  private getLabel_(label: string): string {
    return label;
  }

  /**
   * Prevents on-click handles on the control from being activated when the
   * indicator is clicked.
   */
  private onIndicatorClick_(e: Event): void {
    e.preventDefault();
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [MultideviceRadioButtonElement.is]: MultideviceRadioButtonElement;
  }
}

customElements.define(
    MultideviceRadioButtonElement.is, MultideviceRadioButtonElement);
