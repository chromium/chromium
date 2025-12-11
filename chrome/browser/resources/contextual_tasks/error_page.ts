// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './error_page.css.js';
import {getHtml} from './error_page.html.js';

export class ContextualTasksErrorPageElement extends CrLitElement {
  static get is() {
    return 'error-page';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {};
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'error-page': ContextualTasksErrorPageElement;
  }
}

customElements.define(
    ContextualTasksErrorPageElement.is, ContextualTasksErrorPageElement);
