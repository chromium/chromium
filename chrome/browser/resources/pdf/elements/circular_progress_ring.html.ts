// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CircularProgressRingElement} from './circular_progress_ring.js';

export function getHtml(this: CircularProgressRingElement) {
  return html`
<svg id="progressRing" viewBox="-25 -25 250 250"
    version="1.1" xmlns="http://www.w3.org/2000/svg">
  <circle id="progressTrack"></circle>
  <circle id="innerProgress"
      stroke-dashoffset="${this.strokeDashOffset}">
  </circle>
</svg>`;
}
