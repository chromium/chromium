// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';

export class ComposeboxElement extends CrLitElement {
  static get is() {
    return 'ntp-composebox';
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
    'ntp-composebox': ComposeboxElement;
  }
}

customElements.define(ComposeboxElement.is, ComposeboxElement);
