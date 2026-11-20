// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsOnDeviceAiPageElement} from './on_device_ai_page.js';

export function getHtml(this: SettingsOnDeviceAiPageElement) {
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{onDeviceAiEnabledLabel}"
    route-path="${this.routePath}">
  <settings-toggle-button id="onDeviceAiToggle"
      pref-key="synthetic.settings.on_device_ai_enabled"
      label="$i18n{onDeviceAiCapabilitiesLabel}"
      sub-label-with-link="$i18n{onDeviceAiCapabilitiesSubLabel}" no-set-pref
      @sub-label-link-clicked="${this.onOnDeviceAiSubLabelLinkClicked_}"
      @settings-boolean-control-change="${
          this.onOnDeviceAiSettingsBooleanControlChange_}">
  </settings-toggle-button>
  ${this.showTurnOffDialog_ ? html`
    <settings-simple-confirmation-dialog id="turnOffDialog"
        title-text="$i18n{onDeviceAiTurnOffDialogTitle}"
        body-text="$i18n{onDeviceAiTurnOffDialogBody}"
        confirm-text="$i18n{onDeviceAiTurnOffDialogConfirmButton}"
        @close="${this.onTurnOffDialogClose_}">
    </settings-simple-confirmation-dialog>
  ` : ''}
  <div class="settings-columned-section">
    <div class="column">
      <h2 class="description-header">$i18n{columnHeadingWhenOn}</h2>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:chrome-product" aria-hidden="true">
          </cr-icon>
          <div class="secondary">$i18n{onDeviceAiWhenOnFeatures}</div>
        </li>
        <li>
          <cr-icon icon="settings20:history" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{onDeviceAiWhenOnAutoUpdates}</div>
        </li>
      </ul>
    </div>
    <div class="column">
      <h2 class="description-header">$i18n{columnHeadingConsider}</h2>
      <ul class="icon-bulleted-list">
        <li>
          <cr-icon icon="settings20:block" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{onDeviceAiConsiderWhenOff}</div>
        </li>
        <li>
          <cr-icon icon="settings20:download" aria-hidden="true"></cr-icon>
          <div class="secondary">$i18n{onDeviceAiConsiderStorage}</div>
        </li>
      </ul>
    </div>
  </div>

  <div class="models-section">
    <h2 id="modelsHeader" class="cr-title-text">
      $i18n{onDeviceAiModelsHeader}
    </h2>
    <div class="models-description secondary">
      $i18n{onDeviceAiModelsDescription}
    </div>
    ${this.capabilities_.length > 0 ? html`
      <settings-focus-list-container id="modelsList" role="list"
          aria-labelledby="modelsHeader">
        ${repeat(this.capabilities_, item => item.id, item => html`
          <settings-on-device-ai-page-entry role="listitem"
              .capability="${item}" @delete-click="${this.onEntryDeleteClick_}">
          </settings-on-device-ai-page-entry>
        `)}
      </settings-focus-list-container>
    ` : html`
      <div id="noModels" class="secondary">
        $i18n{onDeviceAiNoModels}
      </div>
    `}
  </div>
</settings-subpage>
<cr-toast id="toast" duration="5000">
  <div>${this.toastLabel_}</div>
</cr-toast>
<!--_html_template_end_-->`;
}
