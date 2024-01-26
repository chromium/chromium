// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeaturePromoDemoPageInfo} from './user_education_internals.mojom-webui.js';
import {getTemplate} from './user_education_internals_card.html.js';

const PROMO_LAUNCH_EVENT = 'promo-launch';
const CLEAR_PROMO_DATA_EVENT = 'clear-promo-data';

class UserEducationInternalsCardElement extends PolymerElement {
  static get is() {
    return 'user-education-internals-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      promo: Object,

      /**
       * Indicates if the list of instructions is expanded or collapsed.
       */
      instructionsExpanded_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates if the list of promo data is expanded or collapsed.
       */
      dataExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  promo: FeaturePromoDemoPageInfo;
  private instructionsExpanded_: boolean;
  private dataExpanded_: boolean;

  private launchPromo_() {
    this.dispatchEvent(new CustomEvent(
        PROMO_LAUNCH_EVENT,
        {bubbles: true, composed: true, detail: this.promo.internalName}));
  }

  private clearData_() {
    if (confirm(
            'Clear Feature Promo data and Feature Enagement events?\n' +
            'Note: because of session tracking and event constraints, ' +
            'Feature Engagement may still disallow this promo.')) {
      this.dispatchEvent(new CustomEvent(
          CLEAR_PROMO_DATA_EVENT,
          {bubbles: true, composed: true, detail: this.promo.internalName}));
    }
  }

  private showMilestone_() {
    return this.promo.addedMilestone > 0;
  }

  private showDescription_() {
    return this.promo.displayDescription !== '';
  }

  private formatPlatforms_() {
    return this.promo.supportedPlatforms.join(', ');
  }

  private showRequiredFeatures_() {
    return this.promo.requiredFeatures.length;
  }

  private formatRequiredFeatures_() {
    return this.promo.requiredFeatures.join(', ');
  }

  private showInstructions_() {
    return this.promo.instructions.length;
  }

  private showFollowedBy_() {
    return this.promo.followedByInternalName;
  }

  private showData_() {
    return this.promo.data.length;
  }

  private scrollToFollowedBy_() {
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

  private getFollowedByAnchor_() {
    return encodeURIComponent(this.promo.followedByInternalName);
  }
}

customElements.define(
    UserEducationInternalsCardElement.is, UserEducationInternalsCardElement);
