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

const PROMO_LAUNCH_EVENT = 'promo-launch';
const CLEAR_PROMO_DATA_EVENT = 'clear-promo-data';

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
      showAction: {type: Boolean},

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

  promo: FeaturePromoDemoPageInfo|null = null;
  showAction: boolean = false;
  protected instructionsExpanded_: boolean = false;
  protected dataExpanded_: boolean = false;

  protected launchPromo_() {
    assert(this.promo);
    this.dispatchEvent(new CustomEvent(
        PROMO_LAUNCH_EVENT,
        {bubbles: true, composed: true, detail: this.promo.internalName}));
  }

  protected clearData_() {
    assert(this.promo);
    if (confirm(
            'Clear all data associated with this User Education journey?\n' +
            'Note: because of session tracking and event constraints, ' +
            'Feature Engagement may still disallow some IPH.')) {
      this.dispatchEvent(new CustomEvent(
          CLEAR_PROMO_DATA_EVENT,
          {bubbles: true, composed: true, detail: this.promo.internalName}));
    }
  }

  protected showMilestone_() {
    assert(this.promo);
    return this.promo.addedMilestone > 0;
  }

  protected showDescription_() {
    assert(this.promo);
    return this.promo.displayDescription !== '';
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

  protected scrollToFollowedBy_() {
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
