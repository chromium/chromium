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
    ${this.data_?.enablement ? html`
      <table>
        <tr>
          <th>Property</th>
          <th>Value</th>
        </tr>
        <tr>
          <td>Enabled by Chrome Flags</td>
          <td>${!this.data_.enablement.featureDisabled}</td>
        </tr>
        <tr>
          <td>Regular profile</td>
          <td>${!this.data_.enablement.notRegularProfile}</td>
        </tr>
        <tr>
          <td>Pref or flag based rollout (flag or pref) applies</td>
          <td>${!this.data_.enablement.notRolledOut}</td>
        </tr>
        <tr>
          <td>Account exists and has the Gemini in Chrome capability</td>
          <td>${!this.data_.enablement.primaryAccountNotCapable}</td>
        </tr>
        <tr>
          <td>Account exists and is fully signed-in</td>
          <td>${!this.data_.enablement.primaryAccountNotFullySignedIn}</td>
        </tr>
        <tr>
          <td>
            Chrome Enterprise policy allows this feature (or doesn't apply)
          </td>
          <td>${!this.data_.enablement.disallowedByChromePolicy}</td>
        </tr>
        <tr>
          <td>Server side admin allows this feature</td>
          <td>${!this.data_.enablement.disallowedByRemoteAdmin}</td>
        </tr>
        <tr>
          <td>Server side allows this feature (Not admin policy)</td>
          <td>${!this.data_.enablement.disallowedByRemoteOther}</td>
        </tr>
        <tr>
          <td>User did pass the FRE</td>
          <td>${!this.data_.enablement.notConsented}</td>
        </tr>
      </table>` :
      html`<h3 id="loadingMsg">Loading...</h3>`}
    <h2>Sub-features</h2>
    ${this.data_?.enablement ? html`
      <table>
        <tr>
          <th>Feature</th>
          <th>State</th>
        </tr>
        <tr>
          <td>Account is eligible for Live</td>
          <td>${!this.data_.enablement.liveDisallowed}</td>
        </tr>
        <tr>
          <td>Actuation eligibility</td>
          <td>
            ${this.getActuationEligibilityString_(
                this.data_.enablement.actuationEligibility)}
          </td>
        </tr>
      </table>` :
      html`<h3 id="loadingMsg">Loading...</h3>`}
    <h2>Configuration</h2>
    ${this.data_?.config ? html`
      <table>
        <tr>
          <th>Name</th>
          <th>Value</th>
        </tr>
        <tr>
          <td>Guest URL</td>
          <td>${this.data_.config.guestUrl}</td>
        </tr>
        <tr>
          <td>FRE guest URL</td>
          <td>${this.data_.config.freGuestUrl}</td>
        </tr>
      </table>` :
      html`<h3 id="loadingMsg">Loading...</h3>`}
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
