// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

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
    <!-- Only show the menu button for user-created skills. -->
    ${this.isDiscoverCard_() ? html`` : html`
      <cr-icon-button id="moreButton" iron-icon="cr:more-vert"
          @click="${this.onMoreButtonClick_}">
      </cr-icon-button>
      <cr-action-menu id="menu">
        <cr-button class="dropdown-item" id="deleteButton"
            @click="${this.onDeleteButtonClick_}">
          <cr-icon icon="cr:delete" slot="prefix-icon"></cr-icon>
          $i18n{delete}
        </cr-button>
        <cr-button class="dropdown-item" id="copyButton"
            @click="${this.onCopyButtonClick_}">
          <cr-icon icon="skills:copy" slot="prefix-icon"></cr-icon>
          $i18n{copyInstructions}
        </cr-button>
      </cr-action-menu>
    `}
  </div>
  <div id="cardBody">${this.getCardBodyText_()}</div>
  <div id="cardFooter">
    <!-- Show save button for discover cards and edit button for user-created skills. -->
    ${this.isDiscoverCard_() ? html`
      <cr-button id="saveButton" ?disabled="${this.saveDisabled}"
          @click="${this.onSaveButtonClick_}">
        <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
        $i18n{add}
      </cr-button>
    ` : html`
      <cr-button id="editButton" @click="${this.onEditButtonClick_}">
        <cr-icon icon="cr:create" slot="prefix-icon"></cr-icon>
        $i18n{edit}
      </cr-button>
    `}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
