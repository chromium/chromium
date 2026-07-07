// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PermissionDashboardElement} from './permission_dashboard.js';

export function getHtml(this: PermissionDashboardElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container">
  <permission-chip
      id="indicator-chip"
      .chipState="${this.dashboardState?.indicatorChip || null}"
      ?visible="${!!this.dashboardState?.indicatorChip?.isVisible}">
  </permission-chip>

  <permission-chip
      id="request-chip"
      .chipState="${this.dashboardState?.requestChip || null}"
      ?visible="${!!this.dashboardState?.requestChip?.isVisible}"
      ?has-divider="${!!this.dashboardState?.isDividerVisible}">
  </permission-chip>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
