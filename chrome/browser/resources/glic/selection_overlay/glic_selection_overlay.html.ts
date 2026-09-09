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
            >
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
        <region-selection id="regionSelectionLayer"
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

    ${this.enableSelectionOverlayPrompt ? html`
      <div id="floatingPromptContainer"
          ?hidden="${!this.showFloatingPrompt}"
          style="${this.floatingPromptStyle}">
        <div class="searchbox-pill">
          <div class="sparkle-icon">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
              <path d="M12 2C12 7.52 7.52 12 2 12C7.52 12 12 16.48 12 22C12 16.48 16.48 12 22 12C16.48 12 12 7.52 12 2Z" fill="url(#sparkle-grad)"/>
              <defs>
                <linearGradient id="sparkle-grad" x1="2" y1="2" x2="22" y2="22" gradientUnits="userSpaceOnUse">
                  <stop stop-color="#1B6EF3"/>
                  <stop offset="0.5" stop-color="#7C52FF"/>
                  <stop offset="1" stop-color="#E255F2"/>
                </linearGradient>
              </defs>
            </svg>
          </div>
          <input id="promptInput"
              type="text"
              placeholder="$i18n{askGemini}"
              aria-label="$i18n{askGemini}"
              @keydown="${this.onInputKeydown}"
              @pointerdown="${this.onPromptPointerdown}"
              autocomplete="off">
        </div>

        <div class="action-chips-row" @pointerdown="${this.onPromptPointerdown}">
          ${this.suggestedActions.map((action, index) => html`
            <button class="action-chip"
                data-index="${index}"
                @click="${this.onSuggestedActionClick}">
              <span class="chip-icon">
                ${this.getActionIcon(action.title)}
              </span>
              <span class="chip-label">${action.title}</span>
            </button>
          `)}
        </div>
      </div>
    ` : ''}
    <!--_html_template_end_-->`;
  // clang-format on
}
