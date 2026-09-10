// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevUpdateOptionsDialogElement} from './update_options_dialog.js';

export function getHtml(this: IwaDevUpdateOptionsDialogElement) {
  // clang-format off
  return html`
<cr-dialog id="dialog" show-on-attach @cr-dialog-open="${this.onCrDialogOpen_}">
  <div slot="title">
    <img id="app-icon" draggable="false"
         src="chrome://app-icon/${this.app.appId}/20" alt="">
    <span>${this.app.name} • Update Options</span>
  </div>
  <div slot="body">
    <iwa-dev-combobox id="channelCombobox"
        label="Update Channel"
        placeholder="Select or enter channel"
        match-strategy="contains"
        .value="${this.selectedChannel_}"
        .options="${this.channelOptions_}"
        .errorMessage="${this.channelError_}"
        @value-changed="${this.onChannelValueChanged_}">
    </iwa-dev-combobox>
    <iwa-dev-combobox id="pinnedVersionCombobox"
        label="Pinned Version"
        placeholder="Select or enter version"
        clearable
        .value="${this.selectedPinnedVersion_}"
        .options="${this.versionOptions_}"
        .errorMessage="${this.pinnedVersionError_}"
        @value-changed="${this.onPinnedVersionValueChanged_}">
    </iwa-dev-combobox>
    <div class="toggle-container">
      <span id="allowDowngradesLabel">Allow Downgrades</span>
      <cr-toggle id="allowDowngradesToggle"
          aria-labelledby="allowDowngradesLabel"
          ?checked="${this.selectedAllowDowngrades_}"
          @change="${this.onAllowDowngradesChange_}">
      </cr-toggle>
    </div>
    ${this.fetchError_ ? html`
      <div class="error-message" aria-live="polite">${this.fetchError_}</div>
    ` : ''}
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCancelClick_}">
      Cancel
    </cr-button>
    <cr-button class="action-button" @click="${this.onSaveClick_}"
        ?disabled="${this.isSaveDisabled_()}">
      Save
    </cr-button>
  </div>
</cr-dialog>`;
}
