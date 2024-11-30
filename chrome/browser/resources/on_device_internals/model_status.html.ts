// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceInternalsModelStatusElement} from './model_status.js';

export function getHtml(this: OnDeviceInternalsModelStatusElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="cr-centered-card-container">
  <h2>Model Status</h2>
  <div class="card">
    <div class="cr-row first">
      <div class="cr-padded-text">Foundational model is
      ${this.pageData_.baseModelReady ? '' : ' not'} ready for use.</div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
