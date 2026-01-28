// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppListElement} from './app_list.js';

export function getHtml(this: AppListElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
${this.error ? html`
  <div class="error-card">
    <cr-icon icon="cr:warning"></cr-icon>
    <div id="error-message">$i18n{appStatesQueryFailed}</div>
  </div>
` : ''}
${this.shouldShowNoAppsMessage ? html`
  <div id="no-apps-message">$i18n{noAppsFound}</div>
` : ''}
${this.shouldShowTable ? html`
  <div class="card">
    <table>
      <thead>
        <tr>
          <th>$i18n{appColumn}</th>
          <th>$i18n{scope}</th>
          <th>$i18n{versionColumn}</th>
        </tr>
      </thead>
      <tbody>
        ${this.apps.map(app => html`
          <tr>
            <td class="app-name">${app.displayName}</td>
            <td class="scope">
              <scope-icon .scope="${app.scope}"></scope-icon>
            </td>
            <td class="version">${app.version}</td>
          </tr>
        `)}
      </tbody>
    </table>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
