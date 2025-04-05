// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SpEmptyStateElement} from './sp_empty_state.js';

export function getHtml(this: SpEmptyStateElement) {
  return html`<!--_html_template_start_-->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="${this.darkImagePath}">
  <img id="product-logo" srcset="${this.imagePath}" alt="">
</picture>
<div id="heading">${this.heading}</div>
<div id="body">${this.body}</div>
<!--_html_template_end_-->`;
}
