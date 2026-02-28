// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import './icons.html.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './card.css.js';
import {getHtml} from './card.html.js';
import type {Skill} from './skill.mojom-webui.js';
import {SkillSource} from './skill.mojom-webui.js';
import {SkillsDialogType} from './skills.mojom-webui.js';
import {SkillsPageBrowserProxy} from './skills_page_browser_proxy.js';

export enum CardType {
  USER_SKILL_CARD = 'user-skill-card',
  DISCOVER_SKILL_CARD = 'discover-skill-card',
}

export interface SkillCardElement {
  $: {
    name: HTMLElement,
    icon: HTMLElement,
    menu: CrActionMenuElement,
    deleteButton: CrButtonElement,
    copyButton: CrButtonElement,
    moreButton: CrButtonElement,
    saveButton: CrButtonElement,
    editButton: CrButtonElement,
  };
}

export class SkillCardElement extends CrLitElement {
  static get is() {
    return 'skill-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      skill: {type: Object},
      cardType: {type: String},
      saveDisabled: {type: Boolean},
    };
  }

  accessor skill: Skill = {
    id: '',
    sourceSkillId: '',
    name: '',
    icon: '',
    prompt: '',
    // Default to user created since these are added by the user via the UI.
    source: SkillSource.kUserCreated,
    description: '',
    creationTime: {internalValue: 0n},
    lastUpdateTime: {internalValue: 0n},
  };
  accessor cardType: CardType = CardType.USER_SKILL_CARD;
  accessor saveDisabled: boolean = false;

  private proxy_: SkillsPageBrowserProxy = SkillsPageBrowserProxy.getInstance();

  protected ariaLabelForSkill_(prefix: string): string {
    return loadTimeData.getString(prefix) + ' ' + this.skill.name;
  }

  protected isDiscoverCard_(): boolean {
    return this.cardType === CardType.DISCOVER_SKILL_CARD;
  }

  protected getCardBodyText_(): string {
    return this.isDiscoverCard_() ? this.skill.description : this.skill.prompt;
  }

  protected onEditButtonClick_() {
    this.proxy_.handler.openSkillsDialog(
        SkillsDialogType.kEdit, this.skill);
  }

  protected onSaveButtonClick_() {
    this.fire('save-button-click', this.skill);
  }

  protected onMoreButtonClick_(event: MouseEvent) {
    this.$.menu.showAt(event.target as HTMLElement);
  }

  protected onCopyButtonClick_() {
    // TODO: b/481441891 - Add toast/snackbar to let user know copy was
    // successful.
    navigator.clipboard.writeText(this.skill.prompt);
    this.$.menu.close();
  }

  protected onDeleteButtonClick_() {
    this.proxy_.handler.deleteSkill(this.skill.id);
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'skill-card': SkillCardElement;
  }
}

customElements.define(SkillCardElement.is, SkillCardElement);
