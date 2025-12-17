// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {GlicInternalsAppElement} from './glic_internals_app.js';

export function getHtml(this: GlicInternalsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div id="contents" class="tab-contents">
    <h2>Enablement State</h2>
    ${this.enablement_ ? html`
      <table>
        <tr>
          <th>Property</th>
          <th>Value</th>
        </tr>
        <tr>
          <td>Enabled by Chrome Flags</td>
          <td>${!this.enablement_.featureDisabled}</td>
        </tr>
        <tr>
          <td>Regular profile</td>
          <td>${!this.enablement_.notRegularProfile}</td>
        </tr>
        <tr>
          <td>Pref or flag based rollout (flag or pref) applies</td>
          <td>${!this.enablement_.notRolledOut}</td>
        </tr>
        <tr>
          <td>Account has the Gemini in Chrome capability</td>
          <td>${!this.enablement_.primaryAccountNotCapable}</td>
        </tr>
        <tr>
          <td>
            Chrome Enterprise policy allows this feature (or doesn't apply)
          </td>
          <td>${!this.enablement_.disallowedByChromePolicy}</td>
        </tr>
        <tr>
          <td>Server side admin allows this feature</td>
          <td>${!this.enablement_.disallowedByRemoteAdmin}</td>
        </tr>
        <tr>
          <td>Server side allows this feature (Not admin policy)</td>
          <td>${!this.enablement_.disallowedByRemoteOther}</td>
        </tr>
        <tr>
          <td>User did pass the FRE</td>
          <td>${!this.enablement_.notConsented}</td>
        </tr>
      </table>` :
      html`<h3 id="loadingMsg">Loading...</h3>`}
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
