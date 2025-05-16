// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OverlayBorderGlowElement} from './overlay_border_glow.js';

export function getHtml(this: OverlayBorderGlowElement) {
  return html`<div id="borderGlowContainer">
  <svg style="position: absolute; width: 0; height: 0; overflow: hidden">
    <defs>
      <filter id="blur-filter">
        <feGaussianBlur id="blur" in="SourceGraphic" stdDeviation="0.02">
          <animate
            id="blur-animation"
            attributeName="stdDeviation"
            attributeType="XML"
            from="0.02"
            to="0.05"
            dur="117ms"
            begin="0s"
            fill="freeze"
            calcMode="spline"
            keyTimes="0;1"
            keySplines="0 0 0 0.1"
          />

          <animate
            id="blur-animationToResting"
            attributeName="stdDeviation"
            attributeType="XML"
            from="0.05"
            to="0.1"
            dur="1016ms"
            begin="117ms"
            fill="freeze"
            calcMode="spline"
            keyTimes="0;1"
            keySplines="0.2 0 0 1"
          />
        </feGaussianBlur>
      </filter>

      <mask id="roundedFrameMask" maskContentUnits="objectBoundingBox">
        <rect width="1" height="1" fill="white" />

        <rect
          id="mask-hole-rect"
          x=".18"
          y=".18"
          width=".64"
          height="0.64"
          rx="0.25"
          ry="0.25"
          fill="black"
          filter="url(#blur-filter)"
        />
      </mask>
    </defs>
  </svg>
</div>`;
}
