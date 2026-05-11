// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DrivePickerHostAppElement} from './app.js';

export function getHtml(this: DrivePickerHostAppElement) {
  return html`<!--_html_template_start_-->
<iframe src="chrome-untrusted://drive-picker-host/"></iframe>
<!--_html_template_end_-->`;
}
