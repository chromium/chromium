// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './signature_selector.css.js';
import {getHtml} from './signature_selector.html.js';

export class SignatureSelectorElement extends CrLitElement {
  static get is() {
    return 'signature-selector';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onNewSignatureClick_() {
    this.fire('new-signature-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signature-selector': SignatureSelectorElement;
  }
}

customElements.define(SignatureSelectorElement.is, SignatureSelectorElement);
