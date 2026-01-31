// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SkillCardElement} from './card.js';

export function getHtml(this: SkillCardElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="card">
  <div id="cardHeader">
    <div id="infoContainer">
      <div id="icon">${this.skill.icon}</div>
      <div id="name">${this.skill.name}</div>
    </div>
    <cr-icon-button id="moreButton" iron-icon="cr:more-vert">
    </cr-icon-button>
  </div>
  <div id="cardBody">${this.skill.prompt}</div>
  <div id="cardFooter">
    <cr-button id="editButton">
      <cr-icon icon="cr:create" slot="prefix-icon"></cr-icon>
      $i18n{edit}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
