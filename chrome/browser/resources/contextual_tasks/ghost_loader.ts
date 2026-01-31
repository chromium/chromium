// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './ghost_loader.css.js';
import {getHtml} from './ghost_loader.html.js';

export class GhostLoaderElement extends CrLitElement {
  static get is() {
    return 'ghost-loader';
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
}

declare global {
  interface HTMLElementTagNameMap {
    'ghost-loader': GhostLoaderElement;
  }
}

customElements.define(
    GhostLoaderElement.is, GhostLoaderElement);
