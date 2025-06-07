// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OverlayBorderGlowElement} from './overlay_border_glow.js';

export function getHtml(this: OverlayBorderGlowElement) {
  return html`<div id="borderGlowContainer" .style="${this.getBoundsStyles()}">
    <div .style="${
      this.getGradientColorStyles()}" id="gradientColorLayer"></div>
  </div>`;
}
