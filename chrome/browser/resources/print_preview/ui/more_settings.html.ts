// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MoreSettingsElement} from './more_settings.js';

export function getHtml(this: MoreSettingsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div @click="${this.toggleExpandButton_}" actionable>
  <cr-expand-button aria-label="$i18n{moreOptionsLabel}"
      ?expanded="${this.settingsExpandedByUser}"
      @expanded-changed="${this.onSettingsExpandedByUserChanged_}"
      ?disabled="${this.disabled}">
    <div id="label">$i18n{moreOptionsLabel}</div>
  </cr-expand-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
