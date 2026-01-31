// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './icons.html.js';

import {assert} from '//resources/js/assert.js';
import {CrRouter} from '//resources/js/cr_router.js';
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
    };
  }

  protected accessor skills_ = new Map<string, Skill>();
  private proxy_: SkillsPageBrowserProxy = SkillsPageBrowserProxy.getInstance();
  private updateSkillListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    // Once the `callbackRouter` is notified that `updateSkill` is triggered,
    // update the skill in the `skills_` map.
    this.updateSkillListenerId_ =
        this.proxy_.callbackRouter.updateSkill.addListener(
            this.updateSkill_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.updateSkillListenerId_);
    this.proxy_.callbackRouter.removeListener(this.updateSkillListenerId_);
  }

  // TODO(b/475594136): Process multiple updates in batches.
  private updateSkill_(skill: Skill) {
    this.skills_.set(skill.id, skill);
    // Manually update as Lit does not detect changes in a map.
    this.requestUpdate();
  }

  protected onExploreButtonClick_() {
    const path = '/discover-skills';
    // CRRouter sets the path, but doesn't trigger a popstate event, so we need
    // to dispatch a route-click event to update the page.
    CrRouter.getInstance().setPath(path);
    this.fire('route-click', {path});
  }

  protected onAddSkillButtonClick_() {
    SkillsPageBrowserProxy.getInstance().handler.openSkillsDialog(
        SkillsDialogType.kAdd, /*skill=*/ null);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-skills-page': UserSkillsPageElement;
  }
}

customElements.define(UserSkillsPageElement.is, UserSkillsPageElement);
