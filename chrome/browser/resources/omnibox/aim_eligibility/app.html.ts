// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AimEligibilityAppElement} from './app.js';

export function getHtml(this: AimEligibilityAppElement) {
  // clang-format off
  return html`
    <div class="content">
      <div class="check-item ${
          this.getCheckClass_(this.eligibilityState_.isEligible)}">
        <span class="check-value ${
            this.getCheckClass_(this.eligibilityState_.isEligible)}">
          ${this.getEligibilityText_()}
        </span>
      </div>
      <div class="check-label">AIModeSettings Policy:</div>
      <div class="check-item">
        <span class="check-value ${
            this.getCheckClass_(
                this.eligibilityState_.isEligibleByPolicy)}">
          ${this.getPolicyEligibilityText_()}
        </span>
      </div>
      <div class="check-label">ThirdPartyAiChatSettings Policy:</div>
      <div class="check-item">
        <span class="check-value ${
            this.getCheckClass_(
                this.eligibilityState_.isThirdPartyEligibleByPolicy)}">
          ${this.getThirdPartyPolicyEligibilityText_()}
        </span>
      </div>
      <div class="check-label">Default Search Engine:</div>
      <div class="check-item">
        <span class="check-value ${
            this.getCheckClass_(this.eligibilityState_.isEligibleByDse)}">
          ${this.getDseEligibilityText_()}
        </span>
      </div>
      ${this.eligibilityState_.isServerEligibilityEnabled ? html`
        <div class="check-label">Server Eligibility:</div>
        <div class="check-item">
          <span class="check-value ${
              this.getCheckClass_(
                  this.eligibilityState_.isEligibleByServer)}">
            ${this.getServerEligibilityText_()}
          </span>
        </div>
        <div class="check-label">Eligibility Response Source:</div>
        <div class="check-item">
          <span class="check-value">
            ${this.eligibilityState_.eligibilityResponseSource}
          </span>
        </div>
        ${this.eligibilityState_.eligibilityResponseAuthType ? html`
          <div class="check-label">Auth Type:</div>
          <div class="check-item">
            <span class="check-value">
              ${this.eligibilityState_.eligibilityResponseAuthType}
            </span>
          </div>
        ` : ''}
        <div class="check-label">Eligibility Response:</div>
        <div class="check-item">
          <input class="response-input ${this.inputState_}"
              .value="${this.eligibilityState_.eligibilityResponseBase64Encoded}"
              @input="${this.onResponseInput_}"
              placeholder="Base64 encoded server response">
          </input>
          <div class="response-button-row">
            <cr-button @click="${this.onServerRequestClick_}">
              Request
            </cr-button>
            <cr-button
                ?disabled="${!this.eligibilityState_.eligibilityResponseBase64Encoded}"
                @click="${this.onViewResponseClick_}">
              View
            </cr-button>
            <cr-button @click="${this.onDraftResponseClick_}">
              Draft
            </cr-button>
            <cr-button @click="${this.onSaveResponseClick_}">
              Save
            </cr-button>
          </div>
        </div>
      ` : ''}
      <div class="check-label">SearchboxConfig:</div>
      <div class="check-item">
        <cr-button
            ?disabled="${!this.eligibilityState_.searchboxConfigBase64UrlEncoded}"
            @click="${this.onViewSearchboxConfigClick_}">
          View Demo
        </cr-button>
      </div>

      <div class="drive-state-header">Drive State</div>
      ${this.eligibilityState_.driveStatus ? html`
        <div class="check-label">OVERALL: Is Drive Supported?</div>
        <div class="check-item ${
            this.getCheckClass_(this.eligibilityState_.driveStatus.isDriveSupported)}">
          <span class="check-value ${
              this.getCheckClass_(this.eligibilityState_.driveStatus.isDriveSupported)}">
            ${this.getDriveSupportedText_()}
          </span>
        </div>

        <div class="check-label">PEC API (INPUT_TYPE_DRIVE):</div>
        <div class="check-item">
          <span class="check-value ${
              this.getCheckClass_(this.eligibilityState_.driveStatus.isPecEligible)}">
            ${this.getPecEligibleText_()}
          </span>
        </div>

        <div class="check-label">Identity Match:</div>
        <div class="check-item">
          <span class="check-value ${
              this.getCheckClass_(this.eligibilityState_.driveStatus.isIdentityMatch)}">
            ${this.getIdentityMatchText_()}
          </span>
        </div>

        <div class="check-label">Incognito:</div>
        <div class="check-item">
          <span class="check-value ${
              this.getCheckClass_(!this.eligibilityState_.driveStatus.isIncognito)}">
            ${this.getIncognitoText_()}
          </span>
        </div>

        <div class="check-label">Feature Flag (kComposeboxDriveContextMenuOption):</div>
        <div class="check-item">
          <span class="check-value ${
              this.getCheckClass_(this.eligibilityState_.driveStatus.isFeatureFlagEnabled)}">
            ${this.getFeatureFlagText_()}
          </span>
        </div>

        <div class="check-label">Search Content Sharing Policy:</div>
        <div class="check-item">
          <span class="check-value ${
              this.getCheckClass_(this.eligibilityState_.driveStatus.isSearchContentSharingEnabled)}">
            ${this.getSearchSharingText_()}
          </span>
        </div>

        <div class="check-label">Disclaimer Accepted (FACS):</div>
        <div class="check-item">
          <span class="check-value ${this.getDisclaimerClass_()}">
            ${this.getDisclaimerAcceptedText_()}
          </span>
        </div>
      ` : html`
        <div class="check-item">
          <span class="check-value">Loading Drive State...</span>
        </div>
      `}
    </div>
    <div class="footer">
      Last updated: ${this.getLastUpdatedTimestamp_()}
    </div>
  `;
  // clang-format on
}
