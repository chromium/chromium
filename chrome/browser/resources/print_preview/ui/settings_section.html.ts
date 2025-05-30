// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSectionElement} from './settings_section.js';

export function getHtml(this: SettingsSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<slot name="title"></slot>
<slot name="controls"></slot>
<!--_html_template_end_-->`;
  // clang-format on
}
