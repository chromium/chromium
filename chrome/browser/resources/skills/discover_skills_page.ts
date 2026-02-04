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
import {SkillsPageBrowserProxy} from './skills_page_browser_proxy.js';


// The category name for top skills.
const kTopPickCategoryString: string = 'Top Pick';

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

  /* key: category, value: skill */
  protected accessor skills_: Map<string, Skill[]> = new Map();
  protected accessor selectedCategory_: string = '';
  private listenerIds_: number[] = [];
  private proxy_: SkillsPageBrowserProxy = SkillsPageBrowserProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.proxy_.handler.getInitial1PSkills().then(({skillMap}) => {
      this.update1PMap_(skillMap);
    });
    this.listenerIds_ = [
      this.proxy_.callbackRouter.update1PMap.addListener(
          this.update1PMap_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.proxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  protected update1PMap_(skillMap: {[key: string]: Skill[]}) {
    this.skills_ = new Map(Object.entries(skillMap));
    const otherCategories = this.getOtherCategories_();
    this.selectedCategory_ =
        otherCategories.length > 0 ? otherCategories[0]! : '';
  }

  protected isCategorySelected_(category: string): boolean {
    return this.selectedCategory_ === category;
  }

  protected topSkills_(): Skill[] {
    return this.skills_.get(kTopPickCategoryString) || [];
  }

  protected getSelectedSkills_(): Skill[] {
    return this.skills_.get(this.selectedCategory_) || [];
  }

  // Gets all categories that are not tagged top skills.
  protected getOtherCategories_(): string[] {
    return Array.from(this.skills_.keys())
        .filter(category => category !== kTopPickCategoryString);
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
