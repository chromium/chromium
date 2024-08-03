// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OnDeviceInternalsAppElement} from './app.js';

export function getHtml(this: OnDeviceInternalsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h1>On-Device Internals</h1>
<cr-tab-box id="tabbox" hidden
    @selected-index-change="${this.onSelectedIndexChange_}">
  <div slot="tab">Event Logs</div>
  <div slot="tab">Tools</div>
  <div slot="panel" id="event-log">
    <on-device-internals-event-log></on-device-internals-event-log>
  </div>
  <div slot="panel" id="tools">
    <on-device-internals-tools></on-device-internals-tools>
  </div>
</cr-tab-box>
<!--_html_template_end_-->`;
  // clang-format on
}
