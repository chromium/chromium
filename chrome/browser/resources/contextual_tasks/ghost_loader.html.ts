// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {getBottomDelay, getPairDelays, getSecondGroupDelay, ROW_DELAYS} from './ghost_loader.js';
import type {GhostLoaderElement} from './ghost_loader.js';

// clang-format off
export function getHtml(this: GhostLoaderElement) {
  return html`<!--_html_template_start_-->
  <div class="container">
${ROW_DELAYS.map(initialDelay => html`
    <div class="row">
  ${getPairDelays(initialDelay).map(pairDelay => html`
      <div class="line-container-group">
        <div class="line-container tall delay${pairDelay}">
          <div class="line bg"></div>
          <div class="line"><div class="gradient"></div></div>
          <div class="line"><div class="gradient2"></div></div>
          <div class="line"><div class="gradient3"></div></div>
        </div>
        <div class="line-container short delay${getBottomDelay(pairDelay)}">
          <div class="line bg"></div>
          <div class="line"><div class="gradient"></div></div>
          <div class="line"><div class="gradient2"></div></div>
          <div class="line"><div class="gradient3"></div></div>
        </div>
      </div>
      <div class="line-container-group">
        <div class="line-container short delay${getSecondGroupDelay(pairDelay)}">
          <div class="line bg"></div>
          <div class="line"><div class="gradient"></div></div>
          <div class="line"><div class="gradient2"></div></div>
          <div class="line"><div class="gradient3"></div></div>
        </div>
        <div class="line-container tall delay${getBottomDelay(getSecondGroupDelay(pairDelay))}">
          <div class="line bg"></div>
          <div class="line"><div class="gradient"></div></div>
          <div class="line"><div class="gradient2"></div></div>
          <div class="line"><div class="gradient3"></div></div>
        </div>
      </div>
    `)}
    </div>
  `)}
  </div>
  <!--_html_template_end_-->`;
}
// clang-format on
