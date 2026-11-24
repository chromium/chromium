// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsGeicSubpageElement} from './geic_subpage.js';

export function getHtml(this: SettingsGeicSubpageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{geicSectionTitle}"
    route-path="${this.routePath}">
  <div id="introSection" class="section">
    <h2 id="introHeading" class="cr-title-text">$i18n{geicIntroHeading}</h2>
    <div id="introDescription" class="secondary">
      $i18n{geicIntroDescription}
    </div>
  </div>
  <div class="section">
    <h2 class="cr-title-text">$i18n{glicPreferencesSection}</h2>
    <settings-toggle-button id="tabstripButtonToggle"
        pref-key="glic.pinned_to_tabstrip"
        label="$i18n{glicTabstripButtonToggle}"
        sub-label="$i18n{glicTabstripButtonToggleSublabel}">
    </settings-toggle-button>
  </div>
  <div class="section">
    <h2 class="cr-title-text">$i18n{glicDataSection}</h2>
    <settings-toggle-button id="defaultTabAccessToggle"
        pref-key="glic.default_tab_context_enabled"
        label="$i18n{glicDefaultTabAccessToggle}"
        sub-label-with-link="${this.getDefaultTabAccessSubLabel_()}"
        @sub-label-link-clicked="${
            this.onDefaultTabAccessToggleSubLabelLinkClicked_}"
        no-toggle-on-host-click @click="${this.onDefaultTabAccessExpandClick_}">
      <div id="defaultTabAccessToggleActions" class="toggle-actions-container"
          slot="more-actions">
        <cr-expand-button id="defaultTabAccessExpandButton" no-hover
            ?expanded="${this.defaultTabAccessToggleExpanded_}"
            @expanded-changed="${
                this.onDefaultTabAccessToggleExpandedChanged_}"
            aria-label="$i18n{glicDefaultTabAccessToggle}">
        </cr-expand-button>
        <div class="separator"></div>
      </div>
    </settings-toggle-button>
    <cr-collapse id="defaultTabAccessInfoCollapse"
        ?opened="${this.defaultTabAccessToggleExpanded_}">
      <div class="settings-columned-section">
        <div class="column">
          <h2 class="description-header">$i18n{columnHeadingWhenOn}</h2>
          <ul class="icon-bulleted-list">
            <li>
<if expr="not _google_chrome">
              <cr-icon aria-hidden="true" icon="settings20:edit-square">
              </cr-icon>
</if>
<if expr="_google_chrome">
              <cr-icon aria-hidden="true"
                  icon="settings-internal:text-analysis">
              </cr-icon>
</if>
              <div class="secondary">
                $i18n{glicDefaultTabAccessWhenOn1}
              </div>
            </li>
            <li>
<if expr="not _google_chrome">
              <cr-icon aria-hidden="true" icon="settings20:edit-square">
              </cr-icon>
</if>
<if expr="_google_chrome">
              <cr-icon aria-hidden="true"
                  icon="settings-internal:auto-tab-group">
              </cr-icon>
</if>
              <div class="secondary">
                $i18n{glicDefaultTabAccessWhenOn2}
              </div>
            </li>
          </ul>
        </div>
        <div class="column">
          <h2 class="description-header">$i18n{columnHeadingConsider}</h2>
          <ul class="icon-bulleted-list">
            <li>
<if expr="not _google_chrome">
              <cr-icon aria-hidden="true" icon="settings20:web"></cr-icon>
</if>
<if expr="_google_chrome">
              <cr-icon aria-hidden="true"
                  icon="settings-internal:screensaver-auto">
              </cr-icon>
</if>
              <div class="secondary">
                $i18n{glicDefaultTabAccessConsider1}
              </div>
            </li>
          </ul>
        </div>
      </div>
    </cr-collapse>
  </div>
  ${this.showDevicePolicyToggle_ ? html`
    <div id="devicePolicySection" class="hr">
      <div id="devicePolicyRow" class="cr-row first">
        <div class="flex cr-padded-text">
          <div id="devicePolicyLabel">$i18n{geicDevicePolicyToggle}</div>
          <div id="devicePolicySublabel" class="secondary">
            $i18n{geicDevicePolicyToggleSublabel}
          </div>
        </div>
        <cr-button id="devicePolicyButton"
            aria-describedby="devicePolicyLabel devicePolicySublabel"
            ?disabled="${this.devicePolicyButtonDisabled_}"
            @click="${this.onDevicePolicyButtonClick_}">
          $i18n{geicDevicePolicyButton}
        </cr-button>
      </div>
      <div id="devicePolicyConsider" class="settings-columned-section">
        <div class="column">
          <h2 class="description-header">$i18n{columnHeadingConsider}</h2>
          <ul class="icon-bulleted-list">
            <li>
              <cr-icon aria-hidden="true" icon="settings20:draft-spark">
              </cr-icon>
              <div class="secondary">
                $i18n{geicDevicePolicyConsider1}
              </div>
            </li>
            <li>
              <cr-icon aria-hidden="true" icon="settings20:shield-spark">
              </cr-icon>
              <div class="secondary">
                $i18n{geicDevicePolicyConsider2}
              </div>
            </li>
            <li>
              <cr-icon aria-hidden="true" icon="settings20:chat-spark">
              </cr-icon>
              <div class="secondary">$i18n{geicSwitchConsiderChatHistory}</div>
            </li>
          </ul>
        </div>
      </div>
    </div>
  ` : ''}
<if expr="not is_chromeos">
  ${this.shouldShowRelaunchDialog ? html`
    <relaunch-confirmation-dialog @close="${this.onRelaunchDialogClose}">
    </relaunch-confirmation-dialog>
  ` : ''}
</if>
</settings-subpage>
<!--_html_template_end_-->`;
}
