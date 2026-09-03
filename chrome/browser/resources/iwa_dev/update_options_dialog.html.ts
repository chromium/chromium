// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

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
    <div class="dropdown-container">
      <label for="channelInput">Update Channel</label>
      <input id="channelInput"
          list="channelList"
          class="dropdown-select"
          .value="${this.selectedChannel_}"
          @input="${this.onChannelInput_}"
          placeholder="Select or enter channel">
      <datalist id="channelList">
        ${this.channels_.map(item => html`
          <option value="${item.channel}">
            ${item.displayName || item.channel}
          </option>
        `)}
      </datalist>
    </div>
    <div class="dropdown-container">
      <label for="pinnedVersionInput">
        Pinned Version
      </label>
      <div class="input-container">
        <input id="pinnedVersionInput"
            list="pinnedVersionList"
            class="dropdown-select"
            aria-invalid="${this.pinnedVersionError_ ? 'true' : 'false'}"
            aria-errormessage="${
                this.pinnedVersionError_ ? 'pinnedVersionError' : nothing}"
            .value="${this.selectedPinnedVersion_}"
            @input="${this.onPinnedVersionInput_}"
            placeholder="Select or enter version">
        ${this.selectedPinnedVersion_ ? html`
          <cr-icon-button id="clearPinnedVersionButton"
              iron-icon="cr:close"
              title="Clear pinned version"
              aria-label="Clear pinned version"
              @click="${this.onClearPinnedVersionClick_}">
          </cr-icon-button>
        ` : ''}
      </div>
      <datalist id="pinnedVersionList">
        ${this.versions_.map(v => html`
          <option value="${v.version}">
            ${v.version}
          </option>
        `)}
      </datalist>
      ${this.pinnedVersionError_ ? html`
        <div id="pinnedVersionError" class="error-message" aria-live="polite">
          ${this.pinnedVersionError_}
        </div>
      ` : ''}
    </div>
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
