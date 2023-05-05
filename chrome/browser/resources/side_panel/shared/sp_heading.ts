// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import './sp_shared_style.css.js';

import {assert} from '//resources/js/assert_ts.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sp_heading.html.js';

export interface SpHeading {
  $: {
    backButton: HTMLElement,
  };
}

export class SpHeading extends PolymerElement {
  static get is() {
    return 'sp-heading';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      compact: {
        type: Boolean,
        reflectToAttribute: true,
        valuse: false,
      },

      backButtonAriaLabel: String,
      backButtonTitle: String,

      hideBackButton: {
        type: Boolean,
        value: false,
        observer: 'onHideBackButtonChanged_',
      },

      disableBackButton: {
        type: Boolean,
        value: false,
      },
    };
  }

  compact: boolean;
  backButtonAriaLabel: string;
  backButtonTitle: string;
  hideBackButton: boolean;
  disableBackButton: boolean;

  private onHideBackButtonChanged_() {
    if (!this.hideBackButton) {
      assert(this.backButtonAriaLabel);
    }
  }

  private onBackButtonClick_() {
    this.dispatchEvent(new CustomEvent('back-button-click'));
  }

  getBackButton(): HTMLElement {
    return this.$.backButton;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sp-heading': SpHeading;
  }
}

customElements.define(SpHeading.is, SpHeading);
