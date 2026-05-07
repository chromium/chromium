// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {GlicInternalsAppElement} from './glic_internals_app.js';

export function getHtml(this: GlicInternalsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div>
    <cr-tabs id="tabs" .tabNames="${this.tabNames_}"
        .selected="${this.selectedTabIndex_}"
        @selected-changed="${this.onSelectedTabIndexSelectedChanged_}">
    </cr-tabs>
    <div id="general-contents" class="tab-contents"
        ?hidden="${this.selectedTabIndex_ !== 0}">
        <h2>Enablement State</h2>
        ${this.data_?.enablement ? html`
      <table>
        <tr>
          <th>Property</th>
          <th>Value</th>
        </tr>
        ${this.getTableData_().map(item => html`
          <tr>
            <td>${item.label}</td>
            <td class="status-${item.value}">
              ${item.value ? '✅' : '🚫'}
            </td>
          </tr>
        `)}
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
          <td class="status-${!this.data_.enablement.liveDisallowed}">
            ${!this.data_.enablement.liveDisallowed ? '✅' : '🚫'}
          </td>
        </tr>
        <tr>
          <td>Account is eligible for 'Create Image with Gemini'</td>
          <td class="status-${!this.data_.enablement.shareImageDisallowed}">
            ${!this.data_.enablement.shareImageDisallowed ? '✅' : '🚫'}
          </td>
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

      <!-- ================= DEBUG CONTROLS TAB ================= -->
      <div id="debug-controls-contents" class="tab-contents"
          ?hidden="${this.selectedTabIndex_ !== 1}">
        <h2>Debug Controls</h2>
        <div class="presets-container">
          <h3>Panel</h3>
          <div style="display: flex; gap: 16px; align-items: center;">
            <label>
              <input type="checkbox"
                  .checked="${!!this.data_?.showErrorAllowed}"
                  @change="${this.onShowErrorAllowedChange}">
              Allow Showing Errors
            </label>
          </div>
          <h3>Invoke</h3>
          <label for="invokePromptInput">Prompt</label>
          <input id="invokePromptInput" .value="${this.invokePrompt_}"
              @input="${this.onInvokePromptInput_}">
          </input>
          <div style="display: flex; gap: 16px; align-items: center;">
            <label>
              <input type="checkbox" .checked="${this.invokeAutoSubmit_}"
                  @change="${this.onInvokeAutoSubmitChange_}">
              Auto Submit
            </label>
            <label>
              <input type="checkbox" .checked="${this.invokeWaitForPanelOpen_}"
                  @change="${this.onInvokeWaitForPanelOpenChange_}">
              Wait for Panel Open
            </label>
          </div>
          ${this.invokeAutoSubmit_ ? html`
            <div style="display: flex; gap: 16px; align-items: center;">
              <label>
                <input type="checkbox" .checked="${this.invokeShowPanel_}"
                    @change="${this.onInvokeShowPanelChange_}">
                Show Panel
              </label>
            </div>
          ` : html``}
          <label for="invokeInvocationSourceSelect">Invocation Source</label>
          <select id="invokeInvocationSourceSelect"
              .value="${this.invokeInvocationSource_.toString()}"
              @change="${this.onInvokeInvocationSourceChange_}">
            ${this.getInvocationSourceOptions_().map(option => html`
              <option value="${option.value}">${option.name}</option>
            `)}
          </select>
          <label for="invokeFreOverrideSelect">FRE Override</label>
          <select id="invokeFreOverrideSelect"
              .value="${this.invokeFreOverride_.toString()}"
              @change="${this.onInvokeFreOverrideChange_}">
            <option value="0">Unspecified</option>
            <option value="1">TrustFirstText</option>
            <option value="2">TrustFirstClick</option>
            <option value="3">TrustFirstInline</option>
          </select>
          <label for="invokeFeatureModeSelect">Feature Mode</label>
          <select id="invokeFeatureModeSelect"
              .value="${this.invokeFeatureMode_.toString()}"
              @change="${this.onInvokeFeatureModeChange_}">
            ${this.featureModeEnumValues_.map(item => html`
              <option value="${item.value}">${item.name}</option>
            `)}
          </select>
          ${this.invokeFeatureMode_ === 2 ? html`
            <label for="invokeActuationTargetSelect">Actuation Target</label>
            <select id="invokeActuationTargetSelect"
                .value="${this.invokeActuationTarget_.toString()}"
                @change="${this.onInvokeActuationTargetChange_}">
              ${this.actuationTargetEnumValues_.map(item => html`
                <option value="${item.value}">${item.name}</option>
              `)}
            </select>
          ` : html``}

          <div style="display: flex; gap: 16px; align-items: center;">
            <label>
              <input type="checkbox" .checked="${this.invokeZssOverride_}"
                  @change="${this.onInvokeZssOverrideChange_}">
              ZSS Override
            </label>
          </div>
          ${this.invokeZssOverride_ ? html`
            <label for="invokeZssAdditionalContentInput">
              ZSS Additional Content
            </label>
            <input id="invokeZssAdditionalContentInput"
                .value="${this.invokeZssAdditionalContent_}"
                @input="${this.onInvokeZssAdditionalContentInput_}">
            </input>
          ` : html``}

          <div style="display: flex; gap: 8px; align-items: center;">
            <label for="invokeSurfaceTypeSelect">Target Surface</label>
            <select id="invokeSurfaceTypeSelect"
                .value="${this.invokeSurfaceType_}"
                @change="${this.onInvokeSurfaceTypeChange_}">
              <option value="default">Default</option>
              <option value="newTab">New Tab</option>
            </select>
            ${this.invokeSurfaceType_ === 'default' ? html`
              <span style="color: gray;">(Uses this window)</span>
            ` : html``}
            ${this.invokeSurfaceType_ === 'newTab' ? html`
              <label style="display: flex; align-items: center; gap: 4px;">
                <input type="checkbox"
                    .checked="${this.invokeOpenInForeground_}"
                    @change="${this.onInvokeOpenInForegroundChange}">
                Open in Foreground
              </label>
            ` : html``}
          </div>

          <cr-button @click="${this.onTriggerInvokeClick_}">
            Trigger Invoke
          </cr-button>

          <div class="log-container"
              style="margin-top: 10px; padding: 5px; border: 1px solid #ccc;
                     max-height: 200px; overflow-y: auto;
                     font-family: monospace;">
            ${this.invokeLogs_.map(
              log => html`<pre style="margin: 0;">${log}</pre>`)}
          </div>
          ${this.data_?.experimentalTriggeringEnabled ? html`
            <h3>Experimental Opt-In</h3>
            <div style="display: flex; gap: 16px; align-items: center;">
              <cr-button @click="${this.onExperimentalOptInClick_}">
                Show Experimental Opt-In
              </cr-button>
            </div>
          ` : html``}
        </div>
        <h2>Guest URL Presets</h2>
        ${this.data_?.config ? html`
          <div class="presets-container">
            <label for="autopushInput">Autopush</label>
            <input id="autopushInput"
                .value="${this.data_.config.autopushGuestUrl}"
                @change="${this.onAutopushInputChange}">
            </input>
            <label for="stagingInput">Staging</label>
            <input
                id="stagingInput" .value="${this.data_.config.stagingGuestUrl}"
                @change="${this.onStagingInputChange}">
            </input>
            <label for="preprodInput">Preprod</label>
            <input
                id="preprodInput" .value="${this.data_.config.preprodGuestUrl}"
                @change="${this.onPreprodInputChange}">
            </input>
            <label for="prodInput">Prod</label>
            <input id="prodInput" .value="${this.data_.config.prodGuestUrl}"
                @change="${this.onProdInputChange}">
            </input>
            <div id="inputErrorMsg" class="hiddenElement">
                Invalid URL submitted: presets not updated
            </div>
            <cr-button @click="${this.onSavePresetsClick_}">Save</cr-button>
          </div>` :
          html`<h3 id="loadingMsg">Loading...</h3>`}
        <h2>Web Continuity URL Preset</h2>
        ${this.data_?.config ? html`
          <div class="web-continuity-container">
            <label for="webContinuityInput">Web Continuity</label>
            <input id="webContinuityInput"
                .value="${this.data_.config.webContinuityOriginatingHostUrl}"
                @change="${this.onWebContinuityInputChange}">
            </input>
            <div id="webContinuityInputErrorMsg" class="hiddenElement">
                Invalid URL submitted: presets not updated
            </div>
            <cr-button @click="${this.onSaveWebContinuityPresetClick_}">
                Save
            </cr-button>
          </div>` :
          html`<h3 id="loadingMsg">Loading...</h3>`}
      </div>
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
