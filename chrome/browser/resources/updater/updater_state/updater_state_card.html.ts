// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UpdaterStateCardElement} from './updater_state_card.js';

export function getHtml(this: UpdaterStateCardElement) {
  assert(
      this.scope !== undefined && this.version !== undefined &&
      this.installPath !== undefined);
  // clang-format off
  return html`
<!--_html_template_start_-->
<div class="card">
  <div class="row">
    <h3 class="label">${this.headingLabel}</h3>
    <div class="value">
      <scope-icon scope="${this.scope}"></scope-icon>
    </div>
  </div>
  <div class="row">
    <div class="label">$i18n{versionLabel}</div>
    <div class="value">${this.version}</div>
  </div>
  <div class="row">
    <div class="label">$i18n{lastChecked}</div>
    <div class="value">
      ${this.lastChecked !== null ? html`
        <div>
          ${this.formattedLastChecked}
        </div>
        <div>
          ${this.formattedLastCheckedRelative}
        </div>
      ` : '$i18n{never}'}
    </div>
  </div>
  <div class="row">
    <div class="label">$i18n{lastStarted}</div>
    <div class="value">
      ${this.lastStarted !== null ? html`
        <div>
          ${this.formattedLastStarted}
        </div>
        <div>
          ${this.formattedLastStartedRelative}
        </div>
      ` : '$i18n{never}'}
    </div>
  </div>
  <div class="row">
    <div class="label">$i18n{installPath}</div>
    <a class="value" href="#" @click="${this.onInstallPathClick}"
        is="action-link" tabindex="0">
      ${this.installPath}
    </a>
  </div>
  ${this.inactiveVersions.length > 0 ? html`
    <div class="row">
      <div class="label">$i18n{inactiveVersions}</div>
      <div class="value">
        <ul>
          ${this.inactiveVersions.map(version => html`<li>${version}</li>`)}
        </ul>
      </div>
    </div>
  ` : ''}
  <div class="spacer"></div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
