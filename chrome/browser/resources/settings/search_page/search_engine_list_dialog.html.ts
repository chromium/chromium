// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SearchEngineListDialogElement} from './search_engine_list_dialog.js';

export function getHtml(this: SearchEngineListDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" @cancel="${this.onDialogCancel_}" show-on-attach>
  <div slot="title">
    <div class="title">$i18n{searchPageTitle}</div>
    <div class="subtitle">
      $i18n{searchEnginesSettingsDialogSubtitle}
    </div>
  </div>
  <div slot="body" class="dialog-body">
    <cr-radio-group .selected="${this.selectedEngineId_}"
        @selected-changed="${this.onRadioGroupSelectedChanged_}">
      ${this.searchEngines.map(item => html`
        <cr-radio-button class="label-first" name="${item.id}">
          <div class="search-engine">
            <settings-search-engine-icon .engine="${item}">
            </settings-search-engine-icon>
            ${item.name}
          </div>
        </cr-radio-button>
      `)}
    </cr-radio-group>
  </div>
  <div slot="button-container">
    ${this.showSaveGuestChoice_ ? html`
      <cr-checkbox id="saveGuestChoiceCheckbox"
          ?checked="${!!this.saveGuestChoice_}"
          @checked-changed="${this.onSaveGuestChoiceCheckedChanged_}">
        $i18n{saveGuestChoiceText}
      </cr-checkbox>
    ` : ''}
    <cr-button id="cancelButton" @click="${this.onCancelClick_}">
      $i18n{searchEnginesCancelButton}
    </cr-button>
    <cr-button id="setAsDefaultButton" class="action-button"
        @click="${this.onSetAsDefaultClick_}"
        ?disabled="${!this.searchEngines.length}">
      $i18n{searchEnginesSetAsDefaultButton}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
