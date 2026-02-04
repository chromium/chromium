// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './error_page.css.js';
import {getHtml} from './error_page.html.js';

export class ErrorPageElement extends CrLitElement {
  static get is() {
    return 'error-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'error-page': ErrorPageElement;
  }
}

customElements.define(ErrorPageElement.is, ErrorPageElement);
