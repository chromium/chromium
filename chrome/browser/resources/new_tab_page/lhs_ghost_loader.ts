// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './lhs_ghost_loader.css.js';
import {getHtml} from './lhs_ghost_loader.html.js';

/**
 * @fileoverview Skeleton of the expanded LHS that <cr-left-hand-side> shows
 * in hybrid mode while the expanded LHS iframe loads. The markup, styles and
 * thread line timings are copied from Search. Keep in sync. Mapping doc:
 * go/ntp-lhs-ghost-loader.
 */

// Search: google3/java/com/google/search/next/aim/ui/threads/v1p5/
// loading_ghost_bars.sfc.tsx
const BAR_HEIGHT_PX = '18px';
const BAR_BORDER_RADIUS_PX = '9px';
const GHOST_BAR_WIDTHS = [85, 75, 90, 80, 95, 78, 88, 82, 92, 85];
const LOADING_BAR_INDICES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];

// Search: google3/java/com/google/search/next/aim/ui/loadingbar/
// loading_bar.sfc.tsx
const LINE_STAGGER_MS = 200;
const LAYER_1_BASE_DELAY_MS = -1600;
const LAYER_2_BASE_DELAY_MS = -5600;
const LAYER_3_BASE_DELAY_MS = -3600;

// NTP: stands in for Search's LoadingBarTdp.
export interface GhostLoadingBar {
  width: string;
  height: string;
  borderRadius: string;
  layerDelaysMs: number[];
}

// Search: LoadingGhostBars() and LoadingBar().
function createGhostLoadingBar(index: number): GhostLoadingBar {
  const baseGap = LINE_STAGGER_MS * index;
  return {
    width: `${GHOST_BAR_WIDTHS[index % GHOST_BAR_WIDTHS.length]}%`,
    height: BAR_HEIGHT_PX,
    borderRadius: BAR_BORDER_RADIUS_PX,
    layerDelaysMs: [
      baseGap + LAYER_1_BASE_DELAY_MS,
      baseGap + LAYER_2_BASE_DELAY_MS,
      baseGap + LAYER_3_BASE_DELAY_MS,
    ],
  };
}

const GHOST_LOADING_BARS: GhostLoadingBar[] =
    LOADING_BAR_INDICES.map(createGhostLoadingBar);

export class LhsGhostLoaderElement extends CrLitElement {
  static get is() {
    return 'ntp-lhs-ghost-loader';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isDark: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  // Selects the dark fallback colors in lhs_ghost_loader.css.
  accessor isDark: boolean = false;
  protected readonly ghostLoadingBars_: GhostLoadingBar[] = GHOST_LOADING_BARS;
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-lhs-ghost-loader': LhsGhostLoaderElement;
  }
}

customElements.define(LhsGhostLoaderElement.is, LhsGhostLoaderElement);
