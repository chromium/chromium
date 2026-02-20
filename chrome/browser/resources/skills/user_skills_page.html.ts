// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {CardType} from './card.js';
import {ErrorType} from './error_page.js';
import type {UserSkillsPageElement} from './user_skills_page.js';

export function getHtml(this: UserSkillsPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.shouldShowNoSearchResults_() ? html`
  <error-page error-type="${ErrorType.NO_SEARCH_RESULTS}"></error-page>` : html`
<div id="header">
  <div id="headerText">
    <h2 id="skillsTitle" class="page-title">$i18n{userSkillsTitle}</h2>
    <p id="subtitle">$i18n{userSkillsDescription}</p>
  </div>
  <cr-button id="addSkillButton" class="action-button"
      ?disabled="${this.addSkillButtonDisabled_}"
      aria-label="$i18n{skillAddNewSkillLabel}"
      @click="${this.onAddSkillButtonClick_}">
    <cr-icon icon="cr:add" slot="prefix-icon"></cr-icon>
    $i18n{add}
  </cr-button>
</div>
${this.filteredSkills_().length === 0 ? html`
  <div id="emptyState">
    <skills-empty></skills-empty>
    <div id="emptyStateText">
      <p class="headline">$i18n{emptyStateTitle}</p>
      <p class="body-text">$i18n{emptyStateDescription}</p>
    </div>
    <cr-button id="browseSkillsButton" class="floating-button"
        @click="${this.onExploreButtonClick_}">
      <cr-icon icon="skills:explore" slot="prefix-icon"></cr-icon>
      $i18n{browseSkillsTitle}
    </cr-button>
  </div>` : html`
   <div class="skill-cards-container">
    ${Array.from(this.filteredSkills_()).map((skill) => html`
      <skill-card .skill="${skill}" .cardType="${CardType.USER_SKILL_CARD}">
      </skill-card>`)}
  </div>`}
`}
<!--_html_template_end_-->`;
  // clang-format on
}
