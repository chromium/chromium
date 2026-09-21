// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAiLoggingInfoBulletElement} from './ai_logging_info_bullet.js';

export function getHtml(this: SettingsAiLoggingInfoBulletElement) {
  return html`<!--_html_template_start_-->
<li>${this.isLoggingDisabledByPolicy_() ?
    html`<cr-policy-pref-indicator id="policyIndicator"
             .pref="${this.pref}"></cr-policy-pref-indicator>` :
    html`<cr-icon icon="settings20:account-box" aria-hidden="true"></cr-icon>`
  }<div class="secondary">${this.getLabel_()}</div></li>
<!--_html_template_end_-->`;
}
