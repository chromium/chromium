// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {GhostLoaderElement} from './ghost_loader.js';

// All delays are calculated modulo this value.
const delayMod = 2000;
// Delay for the tall element of the first group of each row.
const rowDelays = [0, 800, 1600];
// Delay between each group in a row.
const delayBetweenGroups = 200;
// Delays added to each pair of groups in a row. Add elements to add pairs.
const pairDelays = [0, 2 * delayBetweenGroups % delayMod];
// Delay between the top element and the bottom element in a group.
const bottomDelay = 400;

function getpairDelays(initialDelay: number) {
  return pairDelays.map((x) => (initialDelay + x) % delayMod);
}

function getSecondGroupDelay(pairDelay: number) {
  return (pairDelay + delayBetweenGroups) % delayMod;
}

function getBottomDelay(topDelay: number) {
  return (topDelay + bottomDelay) % delayMod;
}

// clang-format off
export function getHtml(this: GhostLoaderElement) {
  return html`<!--_html_template_start_-->
  <div class="container">
${rowDelays.map(initialDelay => html`
    <div class="row">
  ${getpairDelays(initialDelay).map(pairDelay => html`
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
