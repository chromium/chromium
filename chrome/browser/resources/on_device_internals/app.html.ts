// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceInternalsAppElement} from './app.js';

export function getHtml(this: OnDeviceInternalsAppElement) {
  return html`
<h1>On-Device Internals</h1>
<cr-tabs id="tabs" .tabNames="${['Tools', 'Event Logs']}"
    .selected="${this.selectedTabIndex_}"
    @selected-changed="${this.onSelectedIndexChange_}">
</cr-tabs>
<cr-page-selector .selected="${this.selectedTabIndex_}">
  <on-device-internals-tools class="tab-contents"></on-device-internals-tools>
  <on-device-internals-event-log class="tab-contents">
  </on-device-internals-event-log>
</cr-page-selector>`;
}
