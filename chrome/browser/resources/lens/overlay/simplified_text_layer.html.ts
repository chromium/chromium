// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SimplifiedTextLayerElement} from './simplified_text_layer.js';

export function getHtml(this: SimplifiedTextLayerElement) {
  return html`${this.highlightedLines.map(item => html`
  <div .style="${this.getHighlightedLineStyle(item)}" class="highlighted-line">
  `)}`;
}
