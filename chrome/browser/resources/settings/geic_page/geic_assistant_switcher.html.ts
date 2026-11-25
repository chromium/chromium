// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsGeicAssistantSwitcherElement} from './geic_assistant_switcher.js';

export function getHtml(this: SettingsGeicAssistantSwitcherElement) {
  return html`<!--_html_template_start_-->
<div id="section" class="hr">
  <div id="row" class="cr-row first">
    <div class="flex cr-padded-text">
      <div id="label">
        ${this.isEnterpriseTarget_() ? html`
          $i18n{glicSwitchToGeicToggle}
        ` : html`
          $i18n{geicDevicePolicyToggle}
        `}
      </div>
      <div id="sublabel" class="secondary">
        ${this.isEnterpriseTarget_() ? html`
          $i18n{glicSwitchToGeicToggleSublabel}
        ` : html`
          $i18n{geicDevicePolicyToggleSublabel}
        `}
      </div>
    </div>
    <cr-button id="switchButton" aria-describedby="label sublabel"
        ?disabled="${this.buttonDisabled_}"
        @click="${this.onSwitchButtonClick_}">
      $i18n{geicDevicePolicyButton}
    </cr-button>
  </div>
  <div id="consider" class="settings-columned-section">
    <div class="column">
      <h2 class="description-header">$i18n{columnHeadingConsider}</h2>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon aria-hidden="true" icon="${this.getFirstConsiderIcon_()}">
          </cr-icon>
          <div class="secondary">
            ${this.isEnterpriseTarget_() ? html`
              $i18n{glicSwitchToGeicConsider1}
            ` : html`
              $i18n{geicDevicePolicyConsider1}
            `}
          </div>
        </li>
        <li>
          <cr-icon aria-hidden="true" icon="settings20:shield-spark"></cr-icon>
          <div class="secondary">
            ${this.isEnterpriseTarget_() ? html`
              $i18n{glicSwitchToGeicConsider2}
            ` : html`
              $i18n{geicDevicePolicyConsider2}
            `}
          </div>
        </li>
        <li>
          <cr-icon aria-hidden="true" icon="settings20:chat-spark"></cr-icon>
          <div class="secondary">$i18n{geicSwitchConsiderChatHistory}</div>
        </li>
      </ul>
    </div>
  </div>
</div>
<if expr="not is_chromeos">
${this.shouldShowRelaunchDialog ? html`
  <relaunch-confirmation-dialog @close="${this.onRelaunchDialogClose}">
  </relaunch-confirmation-dialog>
` : ''}
</if>
<!--_html_template_end_-->`;
}
