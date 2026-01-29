// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './discover_skills_page.css.js';
import {getHtml} from './discover_skills_page.html.js';
import type {Skill} from './skill.mojom-webui.js';
import {SkillSource} from './skill.mojom-webui.js';

export class DiscoverSkillsPageElement extends CrLitElement {
  static get is() {
    return 'discover-skills-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      skills_: {type: Object},
      selectedCategory_: {type: String},
    };
  }

  /* TODO(b/475594870): Instead of hardcoding, fetch from backend */
  /* key: category, value: skill */
  protected accessor skills_: Map<string, Skill[]> = new Map<string, Skill[]>([
    [
      'Planning',
      [{
        id: '1',
        name: 'test1',
        icon: '',
        prompt: '',
        source: SkillSource.kFirstParty,
      }],
    ],
    [
      'Shopping',
      [{
        id: '2',
        name: 'test2',
        icon: '',
        prompt: '',
        source: SkillSource.kFirstParty,
      }],
    ],
    [
      'Learning',
      [{
        id: '3',
        name: 'test3',
        icon: '',
        prompt: '',
        source: SkillSource.kFirstParty,
      }],
    ],
    [
      'Top',
      [{
        id: '4',
        name: 'test4',
        icon: '',
        prompt: '',
        source: SkillSource.kFirstParty,
      }],
    ],
  ]);
  protected accessor selectedCategory_: string =
      this.skills_.keys().next().value || '';

  protected isCategorySelected_(category: string): boolean {
    return this.selectedCategory_ === category;
  }

  protected topSkills_(): Skill[] {
    return this.skills_.get('Top') || [];
  }

  protected getSelectedSkills_(): Skill[] {
    return this.skills_.get(this.selectedCategory_) || [];
  }

  // Gets all categories that are not tagged top skills.
  protected getOtherCategories_(): string[] {
    return Array.from(this.skills_.keys())
        .filter(category => category !== 'Top');
  }

  protected onCategoryClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const category = target.dataset['category'];
    if (category) {
      this.selectedCategory_ = category;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'discover-skills-page': DiscoverSkillsPageElement;
  }
}

customElements.define(DiscoverSkillsPageElement.is, DiscoverSkillsPageElement);
