// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SelectionOverlayElementElement} from './glic_selection_overlay.js';

export function getHtml(this: SelectionOverlayElementElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="selectionOverlay" @pointerenter="${this.onPointerenter}"
        @pointerleave="${this.onPointerleave}">
      <canvas id="backgroundImageCanvas"
          style="height: ${this.canvasHeight}px; width: ${this.canvasWidth}px;">
      </canvas>
      <!-- Wrapper div is needed to stack the selection elements on top of
          background image. -->
      <div id="selectionElements">
        <!-- Other elements that need to be bounded to the image go here -->
        <div id="extraScrim" ?hidden="${!this.darkenExtraScrim}"></div>
        <overlay-shimmer-canvas id="overlayShimmerCanvas"
            ?hidden="${this.disableShimmer || this.enableBorderGlow}"
            .theme="${this.theme}">
        </overlay-shimmer-canvas>
        ${this.enableBorderGlow ? html`
          <overlay-border-glow id="overlayBorderGlow"
              .selectionOverlayRect="${this.selectionOverlayRect}">
          </overlay-border-glow>
        ` : ''}
        <post-selection-renderer id="postSelectionRenderer"
            .selectionOverlayRect="${this.selectionOverlayRect}"
            .regionSelectedGlowEnabled="${this.enableRegionSelectedGlow}"
            .activeRegionId="${this.activeRegionId}"
            @activate-region="${this.onActivateRegion}"
            background-gradient-hidden>
        </post-selection-renderer>
        <region-selection id="regionSelectionLayer" .theme="${this.theme}"
            .screenshotDataUri="${this.screenshotDataUri}"
            .selectionOverlayRect="${this.selectionOverlayRect}"
            .borderGlowEnabled="${this.enableBorderGlow}"
            region-stroke-color1="#FFFFFF"
            region-stroke-color2="#FFFFFF"
            region-stroke-color3="#FFFFFF"
            region-stroke-color4="#FFFFFF"
            region-stroke-color5="#FFFFFF">
        </region-selection>
      </div>
      <div id="initialFlashScrim"></div>
    </div>
    <div id="cursor"
        class="${this.getHiddenCursorClass(
          this.isPointerInside, this.currentGesture?.state)}">
      <div id="cursorImg"></div>
    </div>
    <!--_html_template_end_-->`;
  // clang-format on
}
