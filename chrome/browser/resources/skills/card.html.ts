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
      <cr-tooltip for="infoContainer" position="bottom" offset="0"
          fit-to-visible-bounds>
        ${this.skill.name}
      </cr-tooltip>
    </div>
    <!-- Only show the menu button for user-created skills. -->
    ${this.isDiscoverCard_() ? html`` : html`
      <cr-icon-button id="moreButton" iron-icon="cr:more-vert"
          aria-label="${this.ariaLabelForSkill_('skillCardActionMenuLabel')}"
          @click="${this.onMoreButtonClick_}">
      </cr-icon-button>
      <cr-action-menu id="menu"
          accessibility-label="$i18n{skillCardActionMenuLabel}"
          role-description="$i18n{menu}">
        <cr-button class="dropdown-item" id="deleteButton"
            @click="${this.onDeleteButtonClick_}"
            aria-label="${this.ariaLabelForSkill_('delete')}">
          <cr-icon icon="cr:delete" slot="prefix-icon"></cr-icon>
          $i18n{delete}
        </cr-button>
        <cr-button class="dropdown-item" id="copyButton"
            @click="${this.onCopyButtonClick_}"
            aria-label="${this.ariaLabelForSkill_('copyInstructions')}">
          <cr-icon icon="skills:copy" slot="prefix-icon"></cr-icon>
          $i18n{copyInstructions}
        </cr-button>
      </cr-action-menu>
    `}
  </div>
  <div id="${this.cardType}Body" class="card-body">${this.getCardBodyText_()}
  </div>
  <div id="cardFooter">
    <!-- Show add for discoverable skills and edit for user skills. -->
    ${this.isDiscoverCard_() ? html`
      <cr-button id="saveButton" ?disabled="${this.saveDisabled}"
          @click="${this.onSaveButtonClick_}"
          aria-label="${this.ariaLabelForSkill_('add')}">
        <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
        $i18n{add}
      </cr-button>
    ` : html`
      <cr-button id="editButton" @click="${this.onEditButtonClick_}"
          aria-label="${this.ariaLabelForSkill_('edit')}">
        <cr-icon icon="cr:create" slot="prefix-icon"></cr-icon>
        $i18n{edit}
      </cr-button>
    `}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
