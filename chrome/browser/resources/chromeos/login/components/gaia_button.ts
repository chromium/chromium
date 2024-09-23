// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping gaia styled button for login/oobe.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';

import {CrButtonElement} from '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './gaia_button.html.js';

export const GaiaButtonBase = mixinBehaviors([], PolymerElement);

export class GaiaButton extends GaiaButtonBase {
  static get is() {
    return 'gaia-button' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      disabled: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      link: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'onLinkChanged',
        value: false,
      },
    };
  }

  disabled: boolean;
  link: boolean;

  private getButton(): CrButtonElement {
    const button = this.shadowRoot?.querySelector('#button');
    assert(button instanceof CrButtonElement);
    return button;
  }

  override focus(): void {
    this.getButton().focus();
  }

  private onLinkChanged(): void {
    this.getButton().classList.toggle('action-button', !this.link);
  }

  private onClick(e: Event): void {
    if (this.disabled) {
      e.stopPropagation();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GaiaButton.is]: GaiaButton;
  }
}

customElements.define(GaiaButton.is, GaiaButton);
