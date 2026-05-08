// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {LocationBarElement} from './location_bar.js';

export function getHtml(this: LocationBarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.locationBarState.lhsChipsState.securityChip.isVisible ? html`
<location-icon .state="${this.locationBarState.lhsChipsState.securityChip}"
    @pointerenter="${this.onChipPointerenter_}"
    @pointerleave="${this.onChipPointerleave_}"
    @pointercancel="${this.onChipPointercancel_}">
</location-icon>` : nothing}
${this.locationBarState.lhsChipsState.permissionDashboard ?
       html`
  <permission-dashboard
    .dashboardState="${
           this.locationBarState.lhsChipsState.permissionDashboard}"
    @pointerenter="${this.onChipPointerenter_}"
    @pointerleave="${this.onChipPointerleave_}"
    @pointercancel="${this.onChipPointercancel_}">
  </permission-dashboard>
` : nothing}
<readonly-omnibox id="omnibox"
  .omniboxViewState="${this.locationBarState.omniboxViewState}">
</readonly-omnibox>
<content-settings-icons id="contentSettings"
    .contentSettingImageStates=
        "${this.locationBarState.contentSettingImageStates}"
    @chip-pointerenter="${this.onChipPointerenter_}"
    @chip-pointerleave="${this.onChipPointerleave_}"
    @chip-pointercancel="${this.onChipPointercancel_}">
</content-settings-icons>
<!--_html_template_end_-->`;
  // clang-format on
}
