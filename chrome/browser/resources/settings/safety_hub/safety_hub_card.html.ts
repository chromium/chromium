// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSafetyHubCardElement} from './safety_hub_card.js';

export function getHtml(this: SettingsSafetyHubCardElement) {
  return html`<!--_html_template_start_-->
<cr-icon id="icon" icon="${this.getStatusIcon_()}"
    class="${this.getColorClass_()}">
</cr-icon>
<div id="header">${this.data.header}</div>
<div id="subheader" class="cr-secondary-text">${this.data.subheader}</div>
<!--_html_template_end_-->`;
}
