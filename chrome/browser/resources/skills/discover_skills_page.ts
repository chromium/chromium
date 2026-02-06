// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import './card.js';

import {EventTracker} from '//resources/js/event_tracker.js';
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
      is1PSkillSaving_: {type: Boolean},
    };
  }

  /* key: category, value: skill */
  protected accessor skills_: Map<string, Skill[]> = new Map();
  protected accessor selectedCategory_: string = '';
  // Determines if a 1P skill is in the process of being saved.
  protected accessor is1PSkillSaving_: boolean = false;
  private listenerIds_: number[] = [];
  private proxy_: SkillsPageBrowserProxy = SkillsPageBrowserProxy.getInstance();
  private eventTracker_: EventTracker = new EventTracker();
  private resetSaveTimeoutId_: number|undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.proxy_.handler.getInitial1PSkills().then(({skillMap}) => {
      this.update1PMap_(skillMap);
    });
    this.listenerIds_ = [
      this.proxy_.callbackRouter.update1PMap.addListener(
          this.update1PMap_.bind(this)),
    ];
    // Listen for save button clicks.
    this.eventTracker_.add(
        document, 'save-button-click', () => this.onSkillSave_());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.proxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
    this.eventTracker_.removeAll();
    if (this.resetSaveTimeoutId_) {
      clearTimeout(this.resetSaveTimeoutId_);
      this.resetSaveTimeoutId_ = undefined;
    }
    this.is1PSkillSaving_ = false;
  }

  protected onSkillSave_() {
    this.is1PSkillSaving_ = true;
    // TODO(b/479029101): Remove this timeout when save logic is implemented.
    this.resetSaveTimeoutId_ = setTimeout(() => {
      this.is1PSkillSaving_ = false;
      this.resetSaveTimeoutId_ = undefined;
    }, 1000);
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
