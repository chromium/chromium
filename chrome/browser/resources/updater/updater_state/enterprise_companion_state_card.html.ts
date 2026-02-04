// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {EnterpriseCompanionStateCardElement} from './enterprise_companion_state_card.js';

export function getHtml(this: EnterpriseCompanionStateCardElement) {
  assert(this.installPath !== undefined);
  // clang-format off
  return html`
<!--_html_template_start_-->
<div class="card">
  <div class="row">
    <h3 class="label">Chrome Enterprise Companion App</h3>
    <div class="value">
      <cr-icon icon="cr20:domain"></cr-icon>
    </div>
  </div>
  <div class="row">
    <div class="label">$i18n{versionLabel}</div>
    <div class="value">${this.version}</div>
  </div>
  <div class="row">
    <div class="label">$i18n{installPath}</div>
    <a class="value" href="#" @click="${this.onInstallPathClick}"
        is="action-link" tabindex="0">
      ${this.installPath}
    </a>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
