// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './toolbar_divider.css.js';
import {getHtml} from './toolbar_divider.html.js';

export class ToolbarDividerElement extends CrLitElement {
  static get is() {
    return 'toolbar-divider';
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
    'toolbar-divider': ToolbarDividerElement;
  }
}

customElements.define(ToolbarDividerElement.is, ToolbarDividerElement);
