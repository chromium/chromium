// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SkillsDialogAppElement} from './skills_dialog_app.js';
import {MAX_NAME_CHAR_COUNT, MAX_PROMPT_CHAR_COUNT, PromptError} from './skills_dialog_app.js';

export function getHtml(this: SkillsDialogAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.shouldShowErrorPage_ ? html`<error-page></error-page>` : html`
  <cr-dialog id="dialog" show-on-attach hide-backdrop @close="${this.onClose_}">
    <div slot="title">
      <h1 id="header">${this.dialogTitle_}</h1>
      <p class="description">$i18n{skillDescription}</p>
    </div>
    <div slot="body" @keydown="${this.onKeydown_}">
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
                placeholder="${this.getNamePlaceholder_()}"
                .value="${this.skill_.name}"
                @value-changed="${this.onNameValueChanged_}"
                @keydown="${this.onNameKeydown_}"
                @focus="${this.onNameFocus_}"
                @blur="${this.onNameBlur_}"
                aria-labelledby="nameLabel"
                maxlength="${MAX_NAME_CHAR_COUNT}"
                ?invalid="${this.hasNameCharLimitError_}">
              <div class="emoji-prefix-container" slot="inline-prefix">
                <cr-icon id="emojiZeroStateIcon"
                    icon="skills:add-reaction" aria-hidden="true"
                    ?hidden="${this.shouldHideEmojiZeroState_()}">
                </cr-icon>
                <input id="emojiTrigger" type="text"
                    class="emoji-trigger ${this.getEmojiTriggerClass_()}"
                    .value="${this.getEmojiTriggerValue_()}"
                    @click="${this.onEmojiBtnClick_}"
                    @input="${this.onEmojiInput_}"
                    @keydown="${this.onEmojiKeydown_}" title="$i18n{chooseIcon}"
                    aria-label="$i18n{chooseIcon}" readonly>
                <div id="generatedPlaceholder"
                    ?hidden="${!this.shouldShowGeneratedPlaceholder_()}">
                  <span id="generatedNameText">${this.generatedName_}</span>
                  <span id="tabHint" aria-hidden="true">Tab</span>
                </div>
              </div>
            </cr-input>
            <div id="nameErrorMessage" class="error-message"
                ?hidden="${!this.hasNameCharLimitError_}">
              $i18n{nameCharLimitError}
            </div>
            ${this.showEmojiPicker_ ? html`
              <skills-emoji-picker id="emojiPicker"
                  @emoji-selected="${this.onEmojiSelected_}"
                  @picker-close="${this.onEmojiPickerClose_}">
              </skills-emoji-picker>
            ` : ''}
            `}
        </div>
        <div id="instructionsWrapper">
          <div id="instructionsLabel" class="cr-form-field-label"
              aria-hidden="true">
            $i18n{instructions}
          </div>
          <div id="textareaWrapper" ?loading="${this.isRefineLoading_}"
            ?error="${this.hasPromptError_()}"
            ?refinement-enabled="${this.isRefinementEnabled_}">
            ${this.isRefineLoading_ ? html`
              <cr-loading-gradient id="instructionsLoader">
                <svg width="100%" height="100%">
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
            ${this.isRefinementEnabled_ ? html`
              <div class="textarea-actions">
                <cr-icon-button id="iconUndo" iron-icon="skills:undo"
                    class="refine-icon" title="$i18n{undo}"
                    aria-label="$i18n{undo}"
                    ?disabled="${this.isUndoDisabled_()}"
                    @click="${this.onUndoClick_}">
                </cr-icon-button>
                <cr-icon-button id="iconRedo" iron-icon="skills:redo"
                    class="refine-icon" title="$i18n{redo}"
                    aria-label="$i18n{redo}"
                    ?disabled="${this.isRedoDisabled_()}"
                    @click="${this.onRedoClick_}">
                </cr-icon-button>
                <cr-icon-button id="iconRefine" iron-icon="skills:refine"
                    class="refine-icon" title="$i18n{refine}"
                    aria-label="$i18n{refine}"
                    ?disabled="${this.isRefineDisabled_()}"
                    @click="${this.onRefineClick_}">
                </cr-icon-button>
              </div>
            ` : ''}
          </div>
          <div id="errorMessage" class="error-message"
              ?hidden="${!this.hasPromptError_()}">
              ${this.promptError_ === PromptError.REFINE ? html`
                $i18n{refineError}
              ` : ''}
              ${this.promptError_ === PromptError.CHAR_LIMIT ? html`
                $i18n{charLimitError}
              ` : ''}
          </div>
        </div>
      <div id="saveErrorContainer" ?hidden="${!this.hasSaveError_}">
        <cr-icon icon="cr:error-outline" class="icon-error"></cr-icon>
        <div id="saveErrorMessage" class="error-message">$i18n{saveError}</div>
      </div>
      <div class="buttons-group">
        <cr-button id="deleteButton" ?hidden="${this.isAddDialog_}"
            @click="${this.onDeleteSkillClick_}">
          $i18n{delete}
        </cr-button>
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
      <div id="accountInfo">$i18n{accountInfo} ${this.signedInEmail_}</div>
    </div>
  </cr-dialog>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
