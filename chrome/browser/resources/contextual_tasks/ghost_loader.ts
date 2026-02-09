// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './ghost_loader.css.js';
import {getHtml} from './ghost_loader.html.js';


// All delays are calculated modulo this value.
const DELAY_MOD = 2000;
// Delay for the tall element of the first group of each row.
export const ROW_DELAYS = [0, 800, 1600];
// Delay between each group in a row.
const DELAY_BETWEEN_GROUPS = 200;
// Delays added to each pair of groups in a row. Add elements to add pairs.
const PAIR_DELAYS = [0, 2 * DELAY_BETWEEN_GROUPS % DELAY_MOD];
// Delay between the top element and the bottom element in a group.
const BOTTOM_DELAY = 400;

export function getPairDelays(initialDelay: number) {
  return PAIR_DELAYS.map((x) => (initialDelay + x) % DELAY_MOD);
}

export function getSecondGroupDelay(pairDelay: number) {
  return (pairDelay + DELAY_BETWEEN_GROUPS) % DELAY_MOD;
}

export function getBottomDelay(topDelay: number) {
  return (topDelay + BOTTOM_DELAY) % DELAY_MOD;
}
export class GhostLoaderElement extends CrLitElement {
  static get is() {
    return 'ghost-loader';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {};
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ghost-loader': GhostLoaderElement;
  }
}

customElements.define(
    GhostLoaderElement.is, GhostLoaderElement);
