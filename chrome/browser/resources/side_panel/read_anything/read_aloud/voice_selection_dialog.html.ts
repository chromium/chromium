// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {VoiceSelectionDialogElement} from './voice_selection_dialog.js';

export function getHtml(this: VoiceSelectionDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="voiceSelectionDialog"
    @close="${this.onDialogClose_}"
    @cancel="${this.onDialogCancel_}"
    show-on-attach
    ignore-popstate>
  <div slot="title" class="voice-menu-title-bar">
    <div class="voice-menu-title">$i18n{voiceSelectionLabel}</div>
  </div>

  <div slot="body" class="voice-menu-body">
    ${this.errorMessages_.map(item => html`
      <p class="notification error-message"
          role="status"
          aria-live="polite">
        ${item}
      </p>
    `)}
    ${this.downloadingMessages_.map(item => html`
      <p class="notification download-message"
          role="status"
          aria-live="polite">
        ${item}
      </p>
    `)}

    <cr-radio-group id="voiceRadioGroup"
        nested-selectable
        aria-label="$i18n{voiceSelectionLabel}"
        .selected="${this.candidateVoice_ ? this.candidateVoice_.name : ''}"
        @selected-changed="${this.onVoiceRadioGroupSelectedChanged_}">
      ${this.voiceGroups_.map((group, groupIndex) => html`
        <div class="lang-group-title"
            role="presentation">${group.language}</div>
        ${group.voices.map((voiceItem, voiceIndex) => html`
          <cr-radio-button
              class="voice-row label-first"
              hide-label-text
              label="${this.voiceLabel_(voiceItem.title)}"
              name="${voiceItem.voice.name}"
              data-test-id="${voiceItem.id}">
            <div class="voice-row-content">
              <!-- LEFT: Play/Stop Preview Button & Adjacent Spinner -->
              <div class="preview-group">
                <cr-icon-button id="preview-icon"
                    tabindex="${this.previewButtonTabIndex_(
                        voiceItem,
                        groupIndex === 0 && voiceIndex === 0)}"
                    class="voice-preview-button"
                    aria-label="${
                      this.previewLabel_(voiceItem.previewInitiated)}"
                    title="${this.previewLabel_(voiceItem.previewInitiated)}"
                    iron-icon="${this.previewIcon_(voiceItem.previewInitiated)}"
                    data-group-index="${groupIndex}"
                    data-voice-index="${voiceIndex}"
                    @click="${this.onVoicePreviewClick_}">
                </cr-icon-button>

                <span class="spinner-span"
                    ?hidden="${this.hideSpinner_(voiceItem)}">
                  <picture class="spinner">
                    <source media="(prefers-color-scheme: dark)"
                        srcset="//resources/images/throbber_small_dark.svg">
                    <img srcset="//resources/images/throbber_small.svg" alt="">
                  </picture>
                </span>
              </div>

              <!-- CENTER: Voice Name -->
              <span class="voice-name">${voiceItem.title}</span>
            </div>
          </cr-radio-button>
        `)}
      `)}
    </cr-radio-group>
  </div>

  <!-- FOOTER: Cancel and Save Action Buttons -->
  <div slot="button-container">
    <cr-button class="cancel-button"
        id="cancelButton"
        @click="${this.onCancelButtonClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button class="action-button"
        id="saveButton"
        @click="${this.onSaveClick_}">
      $i18n{save}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
