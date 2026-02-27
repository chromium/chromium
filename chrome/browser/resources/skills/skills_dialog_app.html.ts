// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SkillsDialogAppElement} from './skills_dialog_app.js';
import {MAX_PROMPT_CHAR_COUNT} from './skills_dialog_app.js';

export function getHtml(this: SkillsDialogAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.shouldShowErrorPage_ ? html`<error-page></error-page>` : html`
  <cr-dialog id="dialog" show-on-attach hide-backdrop @close="${this.onClose_}">
    <div slot="title">
      <h1 id="header">${this.dialogTitle_}</h1>
      <p class="description">$i18n{skillDescription}</p>
    </div>
    <div slot="body">
        <div id="nameWrapper">
          <div id="nameLabel" class="cr-form-field-label" aria-hidden="true">$i18n{name}
          </div>
          ${this.isAutoGenerationLoading_ ? html`
              <div id="nameLoaderContainer">
                <cr-loading-gradient>
                  <svg width="100%" height="38">
                    <clipPath>
                      <circle cx="20" cy="19" r="8"></circle>
                      <rect x="40" y="13" width="100%" height="12" rx="4"></rect>
                    </clipPath>
                  </svg>
                </cr-loading-gradient>
              </div>
            `
            : html`
            <cr-input class="stroked" id="nameText" type="text"
                placeholder="$i18n{namePlaceholder}"
                .value="${this.skill_.name}"
                @value-changed="${this.onNameValueChanged_}"
                aria-labelledby="nameLabel">
              <div class="emoji-prefix-container" slot="inline-prefix">
                <cr-icon id="emojiZeroStateIcon" icon="skills:add-reaction"
                    ?hidden="${this.skill_.icon}" aria-hidden="true">
                </cr-icon>
                <input id="emojiTrigger" class="emoji-trigger" type="text"
                  .value="${this.skill_.icon}" @click="${this.onEmojiBtnClick_}"
                  @input="${this.onEmojiInput_}"
                  @keydown="${this.onEmojiKeydown_}" title="$i18n{chooseIcon}"
                  aria-label="$i18n{chooseIcon}">
              </div>
            </cr-input>
            `}
        </div>
        <div id="instructionsWrapper">
          <div id="instructionsLabel" class="cr-form-field-label"
              aria-hidden="true">
            $i18n{instructions}
          </div>
          <div id="textareaWrapper" ?error="${this.hasRefineError_}"
              ?loading="${this.isRefineLoading_}">
            ${this.isRefineLoading_ ? html`
              <cr-loading-gradient id="instructionsLoader">
                <svg width="100%" height="90">
                  <clipPath>
                    <rect x="10" y="14" width="90%" height="12" rx="4"></rect>
                    <rect x="10" y="38" width="90%" height="12" rx="4"></rect>
                    <rect x="10" y="62" width="60%" height="12" rx="4"></rect>
                  </clipPath>
                </svg>
              </cr-loading-gradient>
            ` : html`
              <textarea id="instructionsText"
                  aria-labelledby="instructionsLabel"
                  maxlength="${MAX_PROMPT_CHAR_COUNT}"
                  placeholder="$i18n{instructionsPlaceholder}"
                  .value="${this.skill_.prompt}"
                  @input="${this.onInstructionsInput_}">
              </textarea>
            `}
            <div class="textarea-actions">
              <cr-icon-button id="iconUndo" iron-icon="skills:undo"
                  class="refine-icon" title="$i18n{undo}"
                  aria-label="$i18n{undo}" ?disabled="${this.isUndoDisabled_()}"
                  @click="${this.onUndoClick_}">
              </cr-icon-button>
              <cr-icon-button id="iconRedo" iron-icon="skills:redo"
                  class="refine-icon" title="$i18n{redo}"
                  aria-label="$i18n{redo}" ?disabled="${this.isRedoDisabled_()}"
                  @click="${this.onRedoClick_}">
              </cr-icon-button>
              <cr-icon-button id="iconRefine" iron-icon="skills:refine"
                  class="refine-icon" title="$i18n{refine}"
                  aria-label="$i18n{refine}"
                  ?disabled="${this.isRefineDisabled_()}"
                  @click="${this.onRefineClick_}">
              </cr-icon-button>
            </div>
          </div>
          <div id="refineErrorMessage" class="error-message"
              ?hidden="${!this.hasRefineError_}">
                $i18n{refineError}
          </div>
        </div>
      <div id="saveErrorContainer" ?hidden="${!this.hasSaveError_}">
        <cr-icon icon="cr:error-outline" class="icon-error"></cr-icon>
        <div id="saveErrorMessage" class="error-message">$i18n{saveError}</div>
      </div>
      <div class="buttons-group">
        <cr-button id="cancelButton" class="cancel-button"
            @click="${this.onCancelClick_}">
          $i18n{cancel}
        </cr-button>
        <cr-button  id="saveButton" class="action-button"
            ?disabled="${this.isSaveButtonDisabled}"
            @click="${this.onSubmitSkillClick_}">
          $i18n{save}
        </cr-button>
      </div>
    </div>
    <div slot="footer">
      <div id="accountInfo">
        <span id="accountLabel">$i18n{accountInfo}</span>
        <span id="accountEmail">${this.signedInEmail_}</span>
      </div>
    </div>
  </cr-dialog>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
