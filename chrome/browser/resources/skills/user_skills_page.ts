// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './card.js';
import './icons.html.js';
import './error_page.js';
import './skills_empty.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {Skill} from './skill.mojom-webui.js';
import {SkillsDialogType} from './skills.mojom-webui.js';
import {SkillsPageBrowserProxy} from './skills_page_browser_proxy.js';
import {getCss} from './user_skills_page.css.js';
import {getHtml} from './user_skills_page.html.js';

export class UserSkillsPageElement extends CrLitElement {
  static get is() {
    return 'user-skills-page';
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
      searchTerm_: {type: String},
      addSkillButtonDisabled_: {type: Boolean},
    };
  }

  // Map tracking skills by id.
  protected accessor skills_: Map<string, Skill> = new Map();
  protected accessor addSkillButtonDisabled_: boolean = false;
  protected accessor searchTerm_: string = '';
  private proxy_: SkillsPageBrowserProxy = SkillsPageBrowserProxy.getInstance();
  private listenerIds_: number[] = [];
  private addSkillButtonDisabledTimer_: number|undefined = undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.proxy_.handler.getInitialUserSkills().then(({skills}) => {
      for (const skill of skills) {
        assert(!this.skills_.has(skill.id));
        this.skills_.set(skill.id, skill);
      }
      // Manually update as Lit does not detect changes in a map.
      this.requestUpdate();
    });

    this.listenerIds_ = [
      this.proxy_.callbackRouter.updateSkill.addListener(
          this.updateSkill_.bind(this)),
      this.proxy_.callbackRouter.removeSkill.addListener(
          this.removeSkill_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.proxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
    if (this.addSkillButtonDisabledTimer_) {
      clearTimeout(this.addSkillButtonDisabledTimer_);
      this.addSkillButtonDisabledTimer_ = undefined;
    }
    this.addSkillButtonDisabled_ = false;
  }

  private updateSkill_(skill: Skill) {
    this.skills_.set(skill.id, skill);
    // Manually update as Lit does not detect changes in a map.
    this.requestUpdate();
  }

  private removeSkill_(skillId: string) {
    this.skills_.delete(skillId);
    // Manually update as Lit does not detect changes in a map.
    this.requestUpdate();
  }

  onSearchChanged(searchTerm: string) {
    this.searchTerm_ = searchTerm;
  }

  protected filteredSkills_(): Skill[] {
    const term = this.searchTerm_.toLowerCase();
    const allSkillsArray = Array.from(this.skills_.values());

    if (!term) {
      return allSkillsArray;
    }

    return allSkillsArray.filter(
        skill => skill.name.toLowerCase().includes(term) ||
            skill.prompt.toLowerCase().includes(term));
  }

  protected shouldShowNoSearchResults_(): boolean {
    return this.filteredSkills_().length === 0 && this.searchTerm_.length > 0;
  }

  protected onExploreButtonClick_() {
    const path = '/browse';
    this.fire('route-click', {path});
  }

  protected onAddSkillButtonClick_() {
    if (this.addSkillButtonDisabled_) {
      return;
    }
    this.addSkillButtonDisabled_ = true;
    SkillsPageBrowserProxy.getInstance().handler.openSkillsDialog(
        SkillsDialogType.kAdd, /*skill=*/ null);
    // Disable the button temporarily to prevent double-clicking.
    // The button will open a dialog to block the page, so we can
    // safely re-enable it after a short period of time.
    this.addSkillButtonDisabledTimer_ = setTimeout(() => {
      this.addSkillButtonDisabled_ = false;
      this.addSkillButtonDisabledTimer_ = undefined;
    }, 1000);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-skills-page': UserSkillsPageElement;
  }
}

customElements.define(UserSkillsPageElement.is, UserSkillsPageElement);
