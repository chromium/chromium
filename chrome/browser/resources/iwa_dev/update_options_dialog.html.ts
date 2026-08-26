// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevUpdateOptionsDialogElement} from './update_options_dialog.js';

export function getHtml(this: IwaDevUpdateOptionsDialogElement) {
  // clang-format off
  return html`
<cr-dialog id="dialog" show-on-attach @cr-dialog-open="${this.onCrDialogOpen_}">
  <div slot="title">Update Options - ${this.app.name}</div>
  <div slot="body">
    <div class="dropdown-container">
      <label for="channelInput">Update Channel</label>
      <input id="channelInput"
          list="channelList"
          class="dropdown-select"
          .value="${this.selectedChannel_}"
          @input="${this.onChannelInput_}"
          ?disabled="${this.isFetching_}"
          placeholder="${this.getChannelPlaceholder_()}">
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
      <input id="pinnedVersionInput"
          list="pinnedVersionList"
          class="dropdown-select"
          .value="${this.selectedPinnedVersion_}"
          @input="${this.onPinnedVersionInput_}"
          ?disabled="${this.isFetching_}"
          placeholder="${this.getVersionPlaceholder_()}">
      <datalist id="pinnedVersionList">
        ${this.versions_.map(v => html`
          <option value="${v.version}">
            ${v.version}
          </option>
        `)}
      </datalist>
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
