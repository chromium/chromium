// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UpdaterStateElement} from './updater_state.js';

export function getHtml(this: UpdaterStateElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
${this.error ? html`
  <div class="error-card">
    <cr-icon icon="cr:warning"></cr-icon>
    <div id="error-message">$i18n{updaterStateQueryFailed}</div>
  </div>
` : ''}
${this.shouldShowSystemUpdaterState() ? html`
  <updater-state-card .scope="${'SYSTEM'}"
      .version="${this.systemUpdaterState.activeVersion}"
      .inactiveVersions="${this.systemUpdaterState.inactiveVersions}"
      .lastChecked="${this.systemUpdaterState.lastChecked}"
      .lastStarted="${this.systemUpdaterState.lastStarted}"
      .installPath="${this.filePathToString(
          this.systemUpdaterState.installationDirectory)}">
  </updater-state-card>
` : ''}
${this.shouldShowUserUpdaterState() ? html`
  <updater-state-card .scope="${'USER'}"
      .version="${this.userUpdaterState.activeVersion}"
      .inactiveVersions="${this.userUpdaterState.inactiveVersions}"
      .lastChecked="${this.userUpdaterState.lastChecked}"
      .lastStarted="${this.userUpdaterState.lastStarted}"
      .installPath="${this.filePathToString(
          this.userUpdaterState.installationDirectory)}">
  </updater-state-card>
` : ''}
${this.shouldShowEnterpriseCompanionState() ? html`
  <enterprise-companion-state-card
      .version="${this.enterpriseCompanionState.version}"
      .installPath="${this.filePathToString(
          this.enterpriseCompanionState.installationDirectory)}">
  </enterprise-companion-state-card>
` : ''}
${this.shouldShowNoUpdatersFound ? html`
  <div id="no-updater-message">$i18n{noUpdaterFound}</div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
