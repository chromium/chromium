// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {WatermarkAppElement} from './app.js';

export function getHtml(this: WatermarkAppElement) {
  return html`
<h1>Watermark</h1>
<div id="watermark">${this.message_}</div>`;
}
