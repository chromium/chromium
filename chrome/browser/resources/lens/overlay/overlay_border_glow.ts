// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './overlay_border_glow.css.js';
import {getHtml} from './overlay_border_glow.html.js';

/*
 * Element responsible for rendering the border glow as a replacement for the
 * shimmer.
 */
export class OverlayBorderGlowElement extends CrLitElement {
  static get is() {
    return 'overlay-border-glow';
  }

  static override get properties() {
    return {};
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
    'overlay-border-glow': OverlayBorderGlowElement;
  }
}

customElements.define(OverlayBorderGlowElement.is, OverlayBorderGlowElement);
