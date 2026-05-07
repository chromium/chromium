// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {PermissionDashboardElement} from './permission_dashboard.js';

export function getHtml(this: PermissionDashboardElement) {
  // clang-format off
  return (!this.dashboardState) ? nothing : html`<!--_html_template_start_-->
<div id="container">
  ${this.dashboardState.indicatorChip?.isVisible ? html`
    <permission-chip
        id="indicator-chip"
        .chipState="${this.dashboardState.indicatorChip}">
    </permission-chip>
  ` : nothing}

  ${this.dashboardState.requestChip?.isVisible ? html`
    <permission-chip
        id="request-chip"
        .chipState="${this.dashboardState.requestChip}">
    </permission-chip>
  ` : nothing}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
