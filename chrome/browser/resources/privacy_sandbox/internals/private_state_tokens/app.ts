// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_input/cr_input.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class PrivateStateTokensAppElement extends CrLitElement {
  static get is() {
    return 'private-state-tokens-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {
        type: Boolean,
        reflect: true,
      },
      myValue: {type: String},
    };
  }

  disabled: boolean = false;
  myValue: string = 'This element is made in lit!';

  // Referenced from the template, so must be protected (not private).
  protected onInputValueChanged_(e: CustomEvent<{value: string}>) {
    this.myValue = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'private-state-tokens-app': PrivateStateTokensAppElement;
  }
}

customElements.define(PrivateStateTokensAppElement.is, PrivateStateTokensAppElement);
