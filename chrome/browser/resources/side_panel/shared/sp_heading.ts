// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons_lit.html.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './sp_heading.css.js';
import {getHtml} from './sp_heading.html.js';

export interface SpHeadingElement {
  $: {
    backButton: HTMLElement,
  };
}

export class SpHeadingElement extends CrLitElement {
  static get is() {
    return 'sp-heading';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      compact: {
        type: Boolean,
        reflect: true,
      },

      backButtonAriaLabel: {type: String},
      backButtonTitle: {type: String},
      hideBackButton: {type: Boolean},
      disableBackButton: {type: Boolean},
    };
  }

  compact: boolean = false;
  backButtonAriaLabel: string;
  backButtonTitle: string;
  hideBackButton: boolean = false;
  disableBackButton: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('hideBackButton')) {
      this.onHideBackButtonChanged_();
    }
  }

  private onHideBackButtonChanged_() {
    if (!this.hideBackButton) {
      assert(this.backButtonAriaLabel);
    }
  }

  protected onBackButtonClick_() {
    this.dispatchEvent(new CustomEvent('back-button-click'));
  }

  getBackButton(): HTMLElement {
    return this.$.backButton;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sp-heading': SpHeadingElement;
  }
}

customElements.define(SpHeadingElement.is, SpHeadingElement);
