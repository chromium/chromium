// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {LhsBrandmarkElement} from './lhs_brandmark.js';

/**
 * @fileoverview 6-letter inline Google wordmark SVG (92x30, G-o-o-g-l-e
 * ordered --i: 0..5). Keep in sync with Search. Mapping doc:
 * go/ntp-lhs-brandmark.
 *
 * Search: google3/gws/output/pages/elements/search_form_logo.html #SRPLogo
 * (googledata/html/images/branding/google_wordmark/v1/svg/
 * googlelogo_color_92x30dp.svg).
 * NTP: uses --i: 0..5 instead of .letter-1..6 classes so lhs_brandmark.css can
 * compute per-letter stagger delays via calc(). Rendered inline rather than via
 * <img> or background-image so Shadow DOM CSS can animate each glyph's
 * transform and dark-mode fill independently.
 */
export function getHtml(this: LhsBrandmarkElement) {
  // clang-format off
  return html`
<svg aria-hidden="true" width="92" height="30" viewBox="0 0 92 30"
    xmlns="http://www.w3.org/2000/svg">
  <path style="--i: 0" fill="#3186ff"
      d="M10.35 23.1c5.89.06 9.9-3.9 9.9-9.98q0-.75-.14-1.7
             h-9.8v2.93h6.87c-.36 3.83-3.06 5.82-6.76 5.82-4.15
             0-7.33-3.12-7.33-7.66 0-4.47 3.03-7.6 7.33-7.6 2.16 0
             3.76.67 5.35 2.3l2.05-2.14c-1.8-2.1-4.4-3.1-7.44-3.1
             -5.9 0-10.49 4.43-10.49 10.5 0 5.8 4.43 10.58 10.46
             10.64">
  </path>
  <path style="--i: 1" fill="#fc413d"
      d="M29.33 23.1c4.41 0 7.6-3.23 7.6-7.72
             0-4.4-3.12-7.67-7.6-7.67-4.23 0-7.57 3-7.57 7.67 0
             4.45 3.15 7.73 7.57 7.73m0-2.68c-2.68 0-4.56-2.2-4.56
             -5.04s1.98-4.99 4.56-4.99c2.73 0 4.57 2.2 4.57 5 0
             2.87-1.88 5.03-4.57 5.03">
  </path>
  <path style="--i: 2" fill="#ffbe00"
      d="M45.76 23.1c4.44 0 7.6-3.24 7.6-7.72
             0-4.4-3.12-7.67-7.6-7.67-4.24 0-7.57 3-7.57 7.67 0
             4.45 3.15 7.73 7.57 7.73m0-2.68c-2.68 0-4.56-2.2-4.56
             -5.04s1.98-4.99 4.56-4.99c2.73 0 4.57 2.2 4.57 5 0
             2.87-1.88 5.03-4.57 5.03">
  </path>
  <path style="--i: 3" fill="#3186ff"
      d="M61.77 29.2q7.18 0 7.17-8.22V8.23h-2.93v1.8h-.06
             c-.92-1.43-2.66-2.23-4.62-2.23-4.24 0-6.82 3.23-6.82
             7.54 0 4.3 2.51 7.46 6.93 7.46a5.5 5.5 0 0 0
             4.47-2.26h.1v1.31c0 2.9-1.5 4.6-4.29 4.6-1.82 0-3.03
             -1-3.92-2.68l-2.66 1.17c1.4 2.96 3.4 4.26 6.63 4.26
             m0-9.2c-2.59 0-4.24-1.9-4.24-4.75 0-2.74 1.64-4.71
             4.25-4.71s4.23 1.83 4.23 4.7c0 2.9-1.7 4.76-4.24
             4.76">
  </path>
  <path style="--i: 4" fill="#00af57" d="M74.3 2.41h-3.08v20.23h3.08z"></path>
  <path style="--i: 5" fill="#fc413d"
      d="M83.4 23.08a7.4 7.4 0 0 0 6.34-3.52l-2.3-1.5a4.8 4.8
             0 0 1-3.9 2.2 4.6 4.6 0 0 1-3.99-2.4l10.33-4.39a7 7 0
             0 0-.28-1.07c-1.23-3.16-3.38-4.6-6.28-4.6-4.42
             0-7.43 3.22-7.43 7.68 0 4.56 3.17 7.6 7.5 7.6
             m-4.51-7.61v-.17c0-2.93 1.68-4.9 4.32-4.9 1.26 0
             2.39.57 3.15 1.9z">
  </path>
</svg>`;
  // clang-format on
}
