// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SkillsDialogAppElement} from './skills_dialog_app.js';

// TODO(b/477385863): Use i18n string once finalized.
export function getHtml(this: SkillsDialogAppElement) {
  // clang-format off
  return html`
<h1 id="header">Add Skill</h1>
<p id="description" class="description">Skills help simplify and automate repetitive tasks</p>
<div class="form-group">
  <cr-input class="no-error stroked" type="text" label="Name"
      placeholder="Simplify For A Kid">
  </cr-input>
  <cr-textarea type="text" label="Instructions"
      placeholder="Example: Simplify this concept for a child who is 8 years old. Use simple language and an analogy they would understand. Keep the total explanation concise, under 150 words.">
  </cr-textarea>
</div>
<div class="buttons-group">
  <cr-button id="cancel-button" class="cancel-button">$i18n{cancel}</cr-button>
  <cr-button id="save-button" class="action-button">$i18n{save}</cr-button>
</div>
`;
  // clang-format on
}
