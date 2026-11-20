// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';

import {TooltipPosition} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from './i18n_setup.js';
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

// NTP: rows of the header and footer placeholders, in the same stagger
// sequence as the thread lines so the shimmer moves from top to bottom.
const LOGO_ROW_INDEX = -3;
const NEW_THREAD_ROW_INDEX = -2;
const SEARCH_THREADS_ROW_INDEX = -1;
const SETTINGS_ROW_INDEX = LOADING_BAR_INDICES.length;

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

// NTP: shimmer layer delays for the header and footer placeholders.
export interface GhostShimmerDelays {
  logo: number[];
  newThread: number[];
  searchThreads: number[];
  settings: number[];
}

// Search: LoadingBar().
function getLayerDelaysMs(index: number): number[] {
  const baseGap = LINE_STAGGER_MS * index;
  return [
    baseGap + LAYER_1_BASE_DELAY_MS,
    baseGap + LAYER_2_BASE_DELAY_MS,
    baseGap + LAYER_3_BASE_DELAY_MS,
  ];
}

// Search: LoadingGhostBars().
function createGhostLoadingBar(index: number): GhostLoadingBar {
  return {
    width: `${GHOST_BAR_WIDTHS[index % GHOST_BAR_WIDTHS.length]}%`,
    height: BAR_HEIGHT_PX,
    borderRadius: BAR_BORDER_RADIUS_PX,
    layerDelaysMs: getLayerDelaysMs(index),
  };
}

const GHOST_LOADING_BARS: GhostLoadingBar[] =
    LOADING_BAR_INDICES.map(createGhostLoadingBar);

const GHOST_SHIMMER_DELAYS: GhostShimmerDelays = {
  logo: getLayerDelaysMs(LOGO_ROW_INDEX),
  newThread: getLayerDelaysMs(NEW_THREAD_ROW_INDEX),
  searchThreads: getLayerDelaysMs(SEARCH_THREADS_ROW_INDEX),
  settings: getLayerDelaysMs(SETTINGS_ROW_INDEX),
};

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
  protected readonly shimmerDelaysMs_: GhostShimmerDelays =
      GHOST_SHIMMER_DELAYS;
  protected readonly closeLabel_: string =
      loadTimeData.getString('aimThreadsCloseSidebarLabel');
  // NTP: cr-tooltip uses physical left/right positions.
  protected readonly closeTooltipPosition_: TooltipPosition =
      isRTL() ? TooltipPosition.LEFT : TooltipPosition.RIGHT;

  override focus(options?: FocusOptions) {
    this.shadowRoot.getElementById('closeButton')!.focus(options);
  }

  // NTP: the close button is functional, unlike the other placeholders, so
  // that the user can close the rail when the expanded LHS iframe does not
  // load.
  protected onCloseClick_() {
    this.fire('close-rail');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-lhs-ghost-loader': LhsGhostLoaderElement;
  }
}

customElements.define(LhsGhostLoaderElement.is, LhsGhostLoaderElement);
