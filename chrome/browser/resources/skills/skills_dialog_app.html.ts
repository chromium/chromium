// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SkillsDialogAppElement} from './skills_dialog_app.js';

// TODO(b/477385863): Use i18n string once finalized.
export function getHtml(this: SkillsDialogAppElement) {
  const MAX_PROMPT_CHAR_COUNT = 20000;
  // clang-format off
  return html`<!--_html_template_start_-->
${this.shouldShowErrorPage_ ? html`<error-page></error-page>` : html`
  <div class="dialog-container">
    <div class="header-container">
      <h1 id="header">Add Skill</h1>
      <p class="description">Skills help simplify and automate repetitive tasks
      </p>
    </div>
    <div class="form-group">
    <div id="label" class="cr-form-field-label" aria-hidden="true">Name</div>
    <cr-input class="no-error stroked" id="nameText" type="text"
          placeholder="Simplify For A Kid" .value="${this.skill_.name}"
          @value-changed="${this.onNameChanged_}">
      <input class="emoji-trigger"
          type="text"
          .value="${this.skill_.icon}"
          @click="${this.onEmojiBtnClick_}"
          @input="${this.onEmojiChanged_}"
          @keydown="${this.onEmojiKeyDown_}"
          title="Choose Icon"
          aria-label="Choose Icon"
          slot="inline-prefix">
    </cr-input>
    <div id="label" class="cr-form-field-label" aria-hidden="true">Instructions
    </div>
    <div class="textarea-wrapper">
      <textarea id="instructionsText" aria-label="Instructions"
          maxlength="${MAX_PROMPT_CHAR_COUNT}"
          placeholder="Example: Simplify this concept for a child who is 8 years
           old. Use simple language and an analogy they would understand.
           Keep the total explanation concise, under ${MAX_PROMPT_CHAR_COUNT}
           words."
          .value="${this.skill_.prompt}"
          @input="${this.onInstructionsChanged_}">
      </textarea>
      <div class="textarea-actions">
        <cr-icon-button class="icon-undo" title="Undo"
            aria-label="Undo"
            ?disabled="${!this.canUndoRefine_}"
            @click="${this.onUndoClick_}">
        </cr-icon-button>
        <cr-icon-button class="icon-redo" title="Redo"
            aria-label="Redo"
            ?disabled="${!this.canRedoRefine_}"
            @click="${this.onRedoClick_}">
        </cr-icon-button>
        <cr-icon-button class="icon-refine" title="Refine"
            aria-label="Refine"
            ?disabled="${this.isRefineDisabled_}"
            @click="${this.onRefineClick_}">
        </cr-icon-button>
      </div>
    </div>
  </div>
  <div id="accountInfo">
    <div id="accountLabel">This skill will save to your Google Account</div>
    <div id="accountEmail">${this.signedInEmail_}</div>
  </div>
  <div class="buttons-group">
    <cr-button id="cancelButton" class="cancel-button" @click="${this.cancel_}">
        $i18n{cancel}
    </cr-button>
    <cr-button id="saveButton" class="action-button"
        ?disabled="${this.isSaveButtonDisabled}" @click="${this.submitSkill_}">
        $i18n{save}
    </cr-button>
  </div>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
