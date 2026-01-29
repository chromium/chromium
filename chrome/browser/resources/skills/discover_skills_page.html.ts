// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DiscoverSkillsPageElement} from './discover_skills_page.js';

// TODO(b/475607224): Instead of hardcoding, add resource strings for
// labels and names.
export function getHtml(this: DiscoverSkillsPageElement) {
  // clang-format off
  /* TODO(b/475606460): Replace this with cards */
  return html`
${this.topSkills_().length > 0 ? html`
<h1 id="top-picks-title" class="page-title">Our top picks</h1>
${this.topSkills_().map(skill => html`<li>${skill.name}</li>`)}` : ''}
<h1 id="discover-skills-title" class="page-title">Discover skills</h1>
<div id="discover-categories">
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
</div>`;
  // clang-format on
}
