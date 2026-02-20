// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {CardType} from './card.js';
import type {DiscoverSkillsPageElement} from './discover_skills_page.js';
import {ErrorType} from './error_page.js';

export function getHtml(this: DiscoverSkillsPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.shouldShowNoSearchResults_() ? html`
  <error-page error-type="${ErrorType.NO_SEARCH_RESULTS}"></error-page>
` : html`
${this.topSkills_().length > 0 ? html`
<h2 class="page-title">$i18n{topPicksTitle}</h2>
<div class="skill-cards-container">
  ${this.topSkills_().map(skill => html`
    <skill-card .skill="${skill}" .cardType="${CardType.DISCOVER_SKILL_CARD}"
        .saveDisabled="${this.shouldDisableSave_(skill)}">
    </skill-card>`)}
</div>` : ''}
${this.getOtherCategories_().length > 0 ? html`
<h2 class="page-title">$i18n{browseSkillsTitle}</h2>
<div id="discoverCategories">
  ${this.getOtherCategories_().map(category => html`
    <cr-chip ?selected="${this.isCategorySelected_(category)}"
        data-category="${category}" @click="${this.onCategoryClick_}">
      <cr-icon icon="${this.isCategorySelected_(category) ? 'cr:check' :
          this.getIconForCategory_(category)}">
      </cr-icon>
      ${category}
    </cr-chip>`)}
</div>
<div class="skill-cards-container">
  ${this.getSelectedSkills_().map(skill => html`
    <skill-card .skill="${skill}" .cardType="${CardType.DISCOVER_SKILL_CARD}"
        .saveDisabled="${this.shouldDisableSave_(skill)}">
    </skill-card>`)}
</div>` : ''}
<cr-toast id="invalidSkillToast">
  <div>$i18n{invalidSkillToastText}</div>
</cr-toast>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
