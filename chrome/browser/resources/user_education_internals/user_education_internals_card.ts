// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {FeaturePromoDemoPageInfo} from './user_education_internals.mojom-webui.js';
import {getCss} from './user_education_internals_card.css.js';
import {getHtml} from './user_education_internals_card.html.js';

const PROMO_ACTION_EVENT = 'promo-action';

export interface PromoAction {
  promo: string;
  key: number;
}

export interface PromoActionDescription {
  caption: string;
  isLaunch: boolean;
  key: number;
  warning?: string;
}

export class UserEducationInternalsCardElement extends CrLitElement {
  static get is() {
    return 'user-education-internals-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      promo: {type: Object},
      actions: {type: Array},

      /**
       * Indicates if the list of instructions is expanded or collapsed.
       */
      instructionsExpanded_: {type: Boolean},

      /**
       * Indicates if the list of promo data is expanded or collapsed.
       */
      dataExpanded_: {type: Boolean},
    };
  }

  accessor promo: FeaturePromoDemoPageInfo = {
    displayTitle: '',
    displayDescription: '',
    internalName: '',
    type: '',
    addedMilestone: 0,
    supportedPlatforms: [],
    instructions: [],
    followedByInternalName: '',
    data: [],
    requiredFeatures: [],
  };
  accessor actions: PromoActionDescription[] = [];
  protected accessor instructionsExpanded_: boolean = false;
  protected accessor dataExpanded_: boolean = false;

  protected onPromoActionClick_(e: Event) {
    assert(this.promo);
    const keyAttr = (e.target as HTMLElement).getAttribute('actionKey');
    assert(keyAttr);
    const key = Number(keyAttr);
    let desc: PromoActionDescription|undefined;
    for (const action of this.actions) {
      if (action.key === key) {
        desc = action;
        break;
      }
    }
    if (!desc || (desc.warning && !confirm(desc.warning))) {
      return;
    }
    this.fire(PROMO_ACTION_EVENT, {promo: this.promo.internalName, key: key});
  }

  protected showMilestone_() {
    assert(this.promo);
    return this.promo.addedMilestone > 0;
  }

  protected showDescription_() {
    assert(this.promo);
    return this.promo.displayDescription !== '';
  }

  protected showType_() {
    assert(this.promo);
    return this.promo.type !== '';
  }

  protected showPlatforms_() {
    assert(this.promo);
    return this.promo.supportedPlatforms.length > 0;
  }

  protected formatPlatforms_() {
    assert(this.promo);
    return this.promo.supportedPlatforms.join(', ');
  }

  protected showRequiredFeatures_() {
    assert(this.promo);
    return this.promo.requiredFeatures.length;
  }

  protected formatRequiredFeatures_() {
    assert(this.promo);
    return this.promo.requiredFeatures.join(', ');
  }

  protected showInstructions_() {
    assert(this.promo);
    return this.promo.instructions.length;
  }

  protected showFollowedBy_() {
    assert(this.promo);
    return this.promo.followedByInternalName;
  }

  protected showData_() {
    assert(this.promo);
    return this.promo.data.length;
  }

  protected getAdditionalActions_(): PromoActionDescription[] {
    const result = [];
    const launchKey = this.getLaunchKey_();
    for (const action of this.actions) {
      if (action.key !== launchKey) {
        result.push(action);
      }
    }
    return result;
  }

  protected getLaunchKey_(): number {
    for (const action of this.actions) {
      if (action.isLaunch) {
        return action.key;
      }
    }
    return -1;
  }

  protected getLaunchCaption_(): string {
    for (const action of this.actions) {
      if (action.isLaunch) {
        return action.caption;
      }
    }
    return '';
  }

  protected showLaunch_() {
    return this.getLaunchKey_() >= 0;
  }

  protected onScrollToFollowedByClick_() {
    assert(this.promo);
    const parent = this.parentElement;
    if (parent) {
      const allCards = parent.querySelectorAll('user-education-internals-card');
      for (const card of allCards) {
        card.classList.remove('highlighted');
      }
      const anchor =
          parent.querySelector(`[id="${this.promo.followedByInternalName}"]`);
      if (anchor) {
        anchor.classList.add('highlighted');
        anchor.scrollIntoView();
      }
    }
  }

  protected getFollowedByAnchor_() {
    assert(this.promo);
    return encodeURIComponent(this.promo.followedByInternalName);
  }

  protected onInstructionsExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.instructionsExpanded_ = e.detail.value;
  }

  protected onDataExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.dataExpanded_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-education-internals-card': UserEducationInternalsCardElement;
  }
}

customElements.define(
    UserEducationInternalsCardElement.is, UserEducationInternalsCardElement);
