// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SiteFaviconElement} from './site_favicon.js';

export function getHtml(this: SiteFaviconElement) {
  return html`<!--_html_template_start_-->
<div id="favicon"
    style="background-image: ${this.getBackgroundImage_()}">
</div>
<!--_html_template_end_-->`;
}
