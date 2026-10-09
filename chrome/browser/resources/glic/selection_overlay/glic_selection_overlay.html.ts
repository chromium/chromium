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
            .hideHandles="${this.hideHandles}"
            .hideCloseButton="${
                this.enableSelectionOverlayPrompt &&
                !this.enableSelectionOverlayPromptBox}"
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
        ${this.enableSelectionOverlayPromptBox ? html`
          <div class="searchbox-pill">
            <div class="sparkle-icon">
              <img src="/spark.svg" width="20" height="20">
            </div>
            <input id="promptInput"
                type="text"
                placeholder="$i18n{askGemini}"
                aria-label="$i18n{askGemini}"
                @keydown="${this.onInputKeydown}"
                @pointerdown="${this.onPromptPointerdown}"
                autocomplete="off">
          </div>
        ` : ''}

        <div class="action-chips-row"
            @pointerdown="${this.onPromptPointerdown}">
          ${this.suggestedActions.map((action, index) => html`
            <button class="action-chip"
                data-index="${index}"
                @click="${this.onSuggestedActionClick}">
              ${this.getActionIcon(action.icon) ? html`
                <span class="chip-icon">
                  <img src="${this.getActionIcon(action.icon)}"
                      width="16" height="16">
                </span>
              ` : ''}
              <span class="chip-label">${action.title}</span>
            </button>
          `)}
          ${!this.enableSelectionOverlayPromptBox ? html`
            <button id="closeRegionButton"
                class="close-region-button"
                title="$i18n{close}"
                aria-label="$i18n{close}"
                @click="${this.onCloseRegionClick}">
              <svg width="16" height="16" viewBox="0 0 24 24"
                  fill="currentColor">
                <path d="M19 6.41 17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5
                    17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/>
              </svg>
            </button>
          ` : ''}
        </div>

        <glic-inline-fulfillment-host id="inlineFulfillmentHost"
            @pointerdown="${this.onPromptPointerdown}">
        </glic-inline-fulfillment-host>
      </div>
    ` : ''}
    <!--_html_template_end_-->`;
  // clang-format on
}
