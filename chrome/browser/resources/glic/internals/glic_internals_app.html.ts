// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {InvocationSource} from '../glic_enums.mojom-webui.js';

import type {GlicInternalsAppElement} from './glic_internals_app.js';

export function getHtml(this: GlicInternalsAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <div>
    <div class="header-bar">
      <cr-tabs id="tabs" .tabNames="${this.tabNames_}"
          .selected="${this.selectedTabIndex_}"
          @selected-changed="${this.onSelectedTabIndexSelectedChanged_}">
      </cr-tabs>
      <div class="action-bar">
        <cr-button id="copyDiagnosticsButton" class="action-button" @click="${this.onCopyDiagnosticsClick_}">
          ${this.copyButtonText_}
        </cr-button>
        <cr-button id="refreshButton" class="tonal-button" @click="${this.onRefreshClick_}">
          Refresh
        </cr-button>
      </div>
    </div>
    <div id="general-contents" class="tab-contents"
        ?hidden="${this.selectedTabIndex_ !== 0}">

      ${this.data_?.enablement ? html`
        <div class="status-banner ${this.computeOverallStatus_().enabled ? 'status-ok' : 'status-blocked'}">
          <div class="status-banner-header">
            <div class="status-banner-title">
              <span class="badge ${this.computeOverallStatus_().enabled ? 'badge-success' : 'badge-error'}">
                ${this.computeOverallStatus_().enabled ? 'Eligible & Enabled' : 'Blocked / Ineligible'}
              </span>
              <span>Glic Status</span>
            </div>
            <div class="status-banner-chips">
              ${this.data_?.debugInfo ? html`
                <span class="badge badge-neutral">${this.getPlatformString_(this.data_.debugInfo.platform)}</span>
                <span class="badge badge-neutral">${this.getFormFactorString_(this.data_.debugInfo.formFactor)}</span>
              ` : ''}
              ${this.data_?.tieredRolloutInfo ? html`
                <span class="badge badge-neutral">Tier: ${this.data_.tieredRolloutInfo.aiSubscriptionTier ?? 'N/A'}</span>
              ` : ''}
            </div>
          </div>
          ${!this.computeOverallStatus_().enabled && this.computeOverallStatus_().reason ? html`
            <div class="status-banner-reason">
              <strong>Blocking reason:</strong> ${this.computeOverallStatus_().reason}
            </div>
          ` : ''}
        </div>
      ` : ''}

      <div class="card">
        <div class="card-header">Enablement State</div>
        ${this.data_?.enablement ? html`
          <table>
            <thead>
              <tr>
                <th class="status-col">Status</th>
                <th>Property</th>
              </tr>
            </thead>
            <tbody>
              ${this.getTableData_().map(item => html`
                <tr>
                  <td class="status-cell-left">
                    <span class="badge ${item.value ? 'badge-success' : 'badge-error'}">
                      ${item.value ? 'Passed' : 'Blocked'}
                    </span>
                  </td>
                  <td class="property-name">${item.label}</td>
                </tr>
              `)}
            </tbody>
          </table>` :
          html`<div style="padding: 16px;">Loading...</div>`}
      </div>

      <div class="card">
        <div class="card-header">Sub-features & Actuation</div>
        ${this.data_?.enablement ? html`
          <table>
            <thead>
              <tr>
                <th class="status-col">State</th>
                <th>Feature</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td class="status-cell-left">
                  <span class="badge ${this.data_.enablement.liveAllowed ? 'badge-success' : 'badge-error'}">
                    ${this.data_.enablement.liveAllowed ? 'Allowed' : 'Disallowed'}
                  </span>
                </td>
                <td class="property-name">Account is eligible for Live</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <span class="badge ${this.data_.enablement.shareImageAllowed ? 'badge-success' : 'badge-error'}">
                    ${this.data_.enablement.shareImageAllowed ? 'Allowed' : 'Disallowed'}
                  </span>
                </td>
                <td class="property-name">Account is eligible for 'Create Image with Gemini'</td>
              </tr>
              <tr>
                <td class="status-cell-left" style="vertical-align: top;">
                  <span class="badge badge-neutral">
                    ${this.data_.enablement.geminiEnterpriseSettings ? 'Configured' : 'None'}
                  </span>
                </td>
                <td class="property-name">
                  <div>Gemini Enterprise Settings</div>
                  ${this.data_.enablement.geminiEnterpriseSettings ? html`
                    <div style="font-size: 12px; color: var(--cr-secondary-text-color); margin-top: 4px;">
                      Project ID: <code>${this.data_.enablement.geminiEnterpriseSettings.projectId}</code> |
                      App ID: <code>${this.data_.enablement.geminiEnterpriseSettings.appId}</code> |
                      Location: <code>${this.data_.enablement.geminiEnterpriseSettings.location}</code>
                    </div>
                  ` : ''}
                </td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <span class="badge ${this.isActuationEligible_() ? 'badge-success' : 'badge-error'}">
                    ${this.getActuationEligibilityString_(this.data_.enablement.actuationEligibility)}
                  </span>
                </td>
                <td class="property-name">Actuation Eligibility</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <span class="badge ${this.isGlicApiActuationEligible_() ? 'badge-success' : 'badge-error'}">
                    ${this.getActuationEligibilityString_(this.data_.enablement.glicApiActuationEligibility)}
                  </span>
                </td>
                <td class="property-name">Glic API Actuation Eligibility</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <span class="badge badge-neutral">
                    ${this.getExperimentalTriggeringStateString_(this.data_.enablement.glicExperimentalTriggeringState)}
                  </span>
                </td>
                <td class="property-name">Experimental Triggering State</td>
              </tr>
            </tbody>
          </table>` :
          html`<div style="padding: 16px;">Loading...</div>`}
      </div>

      <div class="card">
        <div class="card-header">Tiered Rollout / User Tier</div>
        ${this.data_?.tieredRolloutInfo ? html`
          <table>
            <thead>
              <tr>
                <th class="status-col">State / Value</th>
                <th>Property</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td class="status-cell-left">
                  <span class="badge ${this.data_.tieredRolloutInfo.isEligibleOverall ? 'badge-success' : 'badge-error'}">
                    ${this.data_.tieredRolloutInfo.isEligibleOverall ? 'Eligible' : 'Ineligible'}
                  </span>
                </td>
                <td class="property-name">Is Eligible Overall (C++)</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <span class="badge ${this.data_.tieredRolloutInfo.isEligibleForTieredRolloutV1 ? 'badge-success' : 'badge-error'}">
                    ${this.data_.tieredRolloutInfo.isEligibleForTieredRolloutV1 ? 'Eligible' : 'Ineligible'}
                  </span>
                </td>
                <td class="property-name">Is Eligible for Tiered Rollout V1 (C++)</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <span class="badge ${this.data_.tieredRolloutInfo.isEligibleForTieredRolloutV2 ? 'badge-success' : 'badge-error'}">
                    ${this.data_.tieredRolloutInfo.isEligibleForTieredRolloutV2 ? 'Eligible' : 'Ineligible'}
                  </span>
                </td>
                <td class="property-name">Is Eligible for Tiered Rollout V2 (C++)</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <span class="badge ${this.data_.tieredRolloutInfo.glicRolloutEligibilityPref ? 'badge-success' : 'badge-error'}">
                    ${this.data_.tieredRolloutInfo.glicRolloutEligibilityPref ? 'Enabled' : 'Disabled'}
                  </span>
                </td>
                <td class="property-name">Rollout Eligibility Pref (kGlicRolloutEligibility)</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  ${this.data_.tieredRolloutInfo.aiSubscriptionTier === null ?
                      html`<span class="badge badge-neutral">N/A</span>` :
                      html`<span class="badge badge-neutral">Tier ${this.data_.tieredRolloutInfo.aiSubscriptionTier}</span>`}
                </td>
                <td class="property-name">AI Subscription Tier</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <span class="badge badge-neutral">${this.data_.tieredRolloutInfo.preferenceSyncStatus}</span>
                </td>
                <td class="property-name">Preference Sync Status (Server Fetch)</td>
              </tr>
              <tr>
                <td class="status-cell-left">
                  <code>${this.data_.tieredRolloutInfo.tieredRolloutV2EligibleTiers || 'None'}</code>
                </td>
                <td class="property-name">Eligible Tiers for V2 Rollout (Param)</td>
              </tr>
            </tbody>
          </table>` :
          html`<div style="padding: 16px;">Loading...</div>`}
      </div>

      <div class="card">
        <div class="card-header">Configuration</div>
        ${this.data_?.config ? html`
          <table>
            <thead>
              <tr>
                <th style="width: 130px;">Name</th>
                <th>Value</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td style="font-weight: 500;">Guest URL</td>
                <td><code>${this.data_.config.guestUrl}</code></td>
              </tr>
            </tbody>
          </table>` :
          html`<div style="padding: 16px;">Loading...</div>`}
      </div>

      <div class="card">
        <div class="card-header">Glic UI / Client Debug Information</div>
        <div class="warning-banner">
          <span>⚠️</span>
          <span>These settings are not dynamically observed. Click Refresh to get the latest settings.</span>
        </div>
        ${this.data_?.debugInfo ? html`
          <table>
            <thead>
              <tr>
                <th class="status-col">Value</th>
                <th>Setting / Flag</th>
              </tr>
            </thead>
            <tbody>
              ${this.getDebugSettingsData_().map(item => html`
                <tr>
                  <td class="status-cell-left">
                    ${typeof item.value === 'boolean' ?
                      html`<span class="badge ${item.value ? 'badge-success' : 'badge-error'}">${item.value ? 'Enabled' : 'Disabled'}</span>` :
                      html`<span class="badge badge-neutral">${item.value}</span>`}
                  </td>
                  <td class="property-name">${item.label}</td>
                </tr>
              `)}
            </tbody>
          </table>` :
          html`<div style="padding: 16px;">Loading...</div>`}
      </div>
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
          <label for="invokeTimeoutInput">Timeout Override (ms)</label>
          <input id="invokeTimeoutInput" type="number"
              .value="${this.invokeTimeoutMs_}"
              @input="${this.onInvokeTimeoutMsInput_}">
          </input>
          <div style="display: flex; flex-wrap: wrap; gap: 16px;
             align-items: center;">
            <label style="flex: 1 1 calc(50% - 8px);">
              <input type="checkbox" .checked="${this.invokeAutoSubmit_}"
                  @change="${this.onInvokeAutoSubmitChange_}">
              Auto Submit
            </label>
            <label style="flex: 1 1 calc(50% - 8px);">
              <input type="checkbox" .checked="${this.invokeWaitForPanelOpen_}"
                  @change="${this.onInvokeWaitForPanelOpenChange_}">
              Wait for Panel Open
            </label>
            <label style="flex: 1 1 calc(50% - 8px);">
              <input type="checkbox" .checked="${this.invokeFocusOnShow_}"
                  @change="${this.onInvokeFocusOnShowChange_}">
              Focus Panel on Show
            </label>
            <label style="flex: 1 1 calc(50% - 8px);">
              <input type="checkbox"
                  .checked="${this.invokeTakeScreenshot_}"
                  @change="${this.onInvokeTakeScreenshotChange_}">
              Test Take Screenshot
            </label>
            <label style="flex: 1 1 calc(50% - 8px);">
              <input type="checkbox"
                  .checked="${this.invokeSupersedeIfInProgress_}"
                  @change="${this.onInvokeSupersedeIfInProgressChange_}">
              Supersede if In Progress
            </label>
          </div>
          ${this.invokeTakeScreenshot_ ? html`
            <div style="display: flex; flex-direction: column; gap: 8px;
               margin: 8px 0;">
              <label for="invokePublicKeyInput">
                Public Key (Base64 - optional)</label>
              <input id="invokePublicKeyInput" type="text"
                  .value="${this.invokePublicKey_}"
                  @input="${this.onInvokePublicKeyInput_}">
              </input>
              <label for="invokeAuthSecretInput">Auth Secret (optional)</label>
              <input id="invokeAuthSecretInput" type="text"
                  .value="${this.invokeAuthSecret_}"
                  @input="${this.onInvokeAuthSecretInput_}">
              </input>
            </div>
          ` : html``}
          <div style="display: flex; gap: 16px; align-items: center;">
            ${this.invokeAutoSubmit_ ? html`
              <label style="flex: 1;">
                <input type="checkbox" .checked="${this.invokeShowPanel_}"
                    @change="${this.onInvokeShowPanelChange_}">
                Show Panel
              </label>
            ` : html``}
          </div>
          <label for="invokeInvocationSourceSelect">Invocation Source</label>
          <select id="invokeInvocationSourceSelect"
              .value="${this.invokeInvocationSource_.toString()}"
              @change="${this.onInvokeInvocationSourceChange_}">
            ${this.getInvocationSourceOptions_().map(option => html`
              <option value="${option.value}">${option.name}</option>
            `)}
          </select>
          ${this.invokeInvocationSource_ ===
              InvocationSource.kUniversalCart
              ? html`
            <div class="payload-container" style="
                display: flex;
                flex-direction: column;
                gap: 4px;
                margin-top: 8px;">
              <h4>Universal Cart Payload</h4>
              <label for="payloadUniversalCartMetadataInput">
                Serialized Metadata
              </label>
              <input id="payloadUniversalCartMetadataInput"
                  .value="${this.invokePayloadUniversalCartMetadata_}"
                  @input="${this.onPayloadUniversalCartMetadataInput_}">
              </input>
            </div>
          ` : html``}
          <label for="invokeFreOverrideSelect">FRE Override</label>
          <select id="invokeFreOverrideSelect"
              .value="${this.invokeFreOverride_.toString()}"
              @change="${this.onInvokeFreOverrideChange_}">
            <option value="0">Unspecified</option>
            <option value="1">TrustFirstText</option>
            <option value="2">TrustFirstClick</option>
            <option value="3">TrustFirstInline</option>
          </select>
          <label for="invokeFreCompletionWaitModeSelect">
            Wait for FRE Completion Mode
          </label>
          <select id="invokeFreCompletionWaitModeSelect"
              .value="${this.invokeFreCompletionWaitMode_.toString()}"
              @change="${this.onInvokeFreCompletionWaitModeChange_}">
            ${this.freCompletionWaitModeEnumValues_.map(item => html`
              <option value="${item.value}">${item.name}</option>
            `)}
          </select>
          <label for="invokeFeatureModeSelect">Feature Mode</label>
          <select id="invokeFeatureModeSelect"
              .value="${this.invokeFeatureMode_.toString()}"
              @change="${this.onInvokeFeatureModeChange_}">
            ${this.featureModeEnumValues_.map(item => html`
              <option value="${item.value}">${item.name}</option>
            `)}
          </select>
          <label for="invokeActuationTargetSelect">Actuation Target</label>
          <select id="invokeActuationTargetSelect"
              .value="${this.invokeActuationTarget_.toString()}"
              @change="${this.onInvokeActuationTargetChange_}">
            ${this.actuationTargetEnumValues_.map(item => html`
              <option value="${item.value}">${item.name}</option>
            `)}
          </select>

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
              <option value="specificTab">Specific Tab</option>
              <option value="newTab">New Tab</option>
            </select>
            ${this.invokeSurfaceType_ === 'default' ? html`
              <span style="color: gray;">(Uses this window)</span>
            ` : html``}
            ${this.invokeSurfaceType_ === 'specificTab' ? html`
              <select id="invokeSpecificTabIndexSelect"
                  .value="${this.invokeSpecificTabIndex_.toString()}"
                  @change="${this.onInvokeSpecificTabIndexChange_}">
                ${this.availableTabs_.map((tabTitle, index) => html`
                  <option value="${index}">${index}: ${tabTitle}</option>
                `)}
              </select>
              <cr-button @click="${this.onRefreshTabsClick_}">
                Refresh
              </cr-button>
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

          <div style="display: flex; gap: 8px; align-items: flex-start;
                      margin-top: 8px;">
            ${this.invokeSpecificTabsToShareIndices_.length === 0 ? html`
              <cr-button @click="${this.onAddTabsToShareIndexClick_}">
                Add Tabs to Share
              </cr-button>
            ` : html`
              <label style="margin-top: 4px;">Tabs to Share</label>
              <div style="display: flex; flex-direction: column; gap: 4px;">
                ${this.invokeSpecificTabsToShareIndices_.map(
                    (selectedIndex, indexInArray) => html`
                  <div style="display: flex; gap: 4px;">
                    <select .value="${selectedIndex.toString()}"
                        data-index="${indexInArray}"
                        @change="${
                            this.onInvokeSpecificTabsToShareIndexChange_}">
                      ${this.availableTabs_.map((tabTitle, tabIndex) => html`
                        <option value="${tabIndex}">
                          ${tabIndex}: ${tabTitle}
                        </option>
                      `)}
                    </select>
                    <cr-button data-index="${indexInArray}"
                        @click="${this.onRemoveTabsToShareIndexClick_}">
                      x
                    </cr-button>
                  </div>
                `)}
                <cr-button @click="${this.onAddTabsToShareIndexClick_}"
                    style="align-self: flex-start;">
                  +
                </cr-button>
              </div>
              <cr-button @click="${this.onRefreshTabsClick_}">
                Refresh Tabs
              </cr-button>
            `}
          </div>

          <div style="display: flex; gap: 8px; align-items: center;
                      margin-top: 8px;">
            <label for="invokeConversationTypeSelect">
              Target Conversation
            </label>
            <select id="invokeConversationTypeSelect"
                .value="${this.invokeConversationType_}"
                @change="${this.onInvokeConversationTypeChange_}">
              <option value="default">Default</option>
              <option value="new">New Conversation</option>
              <option value="conversationId">Specific Conversation ID</option>
            </select>
            ${this.invokeConversationType_ === 'conversationId' ? html`
              <label for="invokeConversationIdInput"
                     style="margin-left: 8px;">ID:</label>
              <input id="invokeConversationIdInput"
                  .value="${this.invokeConversationId_}"
                  @input="${this.onInvokeConversationIdInput_}">
              </input>
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
            <h3>Consents</h3>
            <div style="display: flex; gap: 16px; align-items: center; flex-wrap: wrap;">
              <cr-checkbox ?checked="${this.data_?.enablement?.freIsConsented}"
                           ?disabled="${!this.data_?.enablement?.freIsConsented}"
                           @change="${this.onGlicConsentChange_}">
                Glic Consent
              </cr-checkbox>
              <cr-checkbox ?checked="${this.data_?.enablement?.actuationIsConsented}"
                           ?disabled="${!this.data_?.enablement?.actuationIsConsented}"
                           @change="${this.onActuationConsentChange_}">
                Actuation Consent
              </cr-checkbox>
              <cr-checkbox ?checked="${this.isExperimentalOptInConsentMet_()}"
                           ?disabled="${!this.isExperimentalOptInConsentMet_()}"
                           @change="${this.onExperimentalConsentChange_}">
                Experimental Triggering Consent
              </cr-checkbox>
              <cr-button ?disabled="${this.isAllConsentMet_()}" @click="${this.onExperimentalOptInClick_}">
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
