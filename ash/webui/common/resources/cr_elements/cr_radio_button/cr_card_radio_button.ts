// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-card-radio-button' is a radio button in the style of a card. A checkmark
 * is displayed in the upper right hand corner if the radio button is selected.
 *
 * Forked from
 * ui/webui/resources/cr_elements/cr_radio_button/cr_card_radio_button.ts
 */
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cr_radio_button_style.css.js';
import '../cr_shared_vars.css.js';
import '../icons.html.js';

import {PaperRippleMixin} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_card_radio_button.html.js';
import {CrRadioButtonMixin} from './cr_radio_button_mixin.js';

const CrCardRadioButtonElementBase =
    PaperRippleMixin(CrRadioButtonMixin(PolymerElement));

export interface CrCardRadioButtonElement {
  $: {
    button: HTMLElement,
  };
}

export class CrCardRadioButtonElement extends CrCardRadioButtonElementBase {
  static get is() {
    return 'cr-card-radio-button';
  }

  static get template() {
    return getTemplate();
  }

  // Overridden from CrRadioButtonMixin
  override getPaperRipple() {
    return this.getRipple();
  }

  // Overridden from PaperRippleMixin
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.shadowRoot!.querySelector('.disc-wrapper');
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-card-radio-button': CrCardRadioButtonElement;
  }
}

customElements.define(CrCardRadioButtonElement.is, CrCardRadioButtonElement);
