// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LocationBarElement} from './location_bar.js';

export function getHtml(this: LocationBarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="permissions-dashboard"></div>
<div id="location-icon"></div>
<readonly-omnibox .omniboxViewState="${this.omniboxViewState}">
</readonly-omnibox>
<!--_html_template_end_-->`;
  // clang-format on
}
