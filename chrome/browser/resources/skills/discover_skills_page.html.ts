// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '/strings.m.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DiscoverSkillsPageElement} from './discover_skills_page.js';

export function getHtml(this: DiscoverSkillsPageElement) {
  // clang-format off
  /* TODO(b/475606460): Replace this with cards */
  return html`<!--_html_template_start_-->
${this.topSkills_().length > 0 ? html`
<h1 class="page-title">$i18n{topPicksTitle}</h1>
${this.topSkills_().map(skill => html`<li>${skill.name}</li>`)}` : ''}
<h1 class="page-title">$i18n{browseSkillsTitle}</h1>
<div id="discoverCategories">
  ${this.getOtherCategories_().map(category => html`
    <cr-chip ?selected="${this.isCategorySelected_(category)}"
        data-category="${category}" @click="${this.onCategoryClick_}">
      <cr-icon icon="${this.isCategorySelected_(category) ?
          'cr:check' : 'cr:add'}">
      </cr-icon>
      ${category}
    </cr-chip>`)}
</div>
<div>
  ${this.getSelectedSkills_().map(skill =>
    /* TODO(b/475606460): Replace this with cards */
    html`<li>${skill.name}</li>`)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
