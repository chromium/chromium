// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContentSettingsIconsElement} from './content_settings_icons.js';

export function getHtml(this: ContentSettingsIconsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.contentSettingImageStates.map(item => html`
  <content-setting-icon .state="${item}">
  </content-setting-icon>
`)}
<!--_html_template_end_-->`;
  // clang-format on
}
