// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SkillsDialogAppElement} from './skills_dialog_app.js';

// TODO(b/477385863): Use i18n string once finalized.
export function getHtml(this: SkillsDialogAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h1 id="header">Add Skill</h1>
<p id="description" class="description">Skills help simplify and automate repetitive tasks</p>
<div class="form-group">
  <cr-input class="no-error stroked" id="nameText" type="text" label="Name"
        placeholder="Simplify For A Kid" .value="${this.skill_.name}"
        @value-changed="${this.onNameChanged_}">
      <input class="emoji-trigger"
          type="text"
          .value="${this.getEmojiDisplay_()}"
          @click="${this.onEmojiBtnClick_}"
          @input="${this.onEmojiChanged_}"
          @keydown="${this.onEmojiKeyDown_}"
          title="Choose Icon"
          aria-label="Choose Icon"
          slot="inline-prefix">
    </cr-input>
  <cr-textarea id="instructionsText" type="text" label="Instructions"
      placeholder="Example: Simplify this concept for a child who is 8 years old. Use simple language and an analogy they would understand. Keep the total explanation concise, under 150 words."
      .value="${this.skill_.prompt}"
      @value-changed="${this.onInstructionsChanged_}">
  </cr-textarea>
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
<!--_html_template_end_-->`;
  // clang-format on
}
