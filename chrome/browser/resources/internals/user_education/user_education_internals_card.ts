// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FeaturePromoDemoPageInfo} from './user_education_internals.mojom-webui.js';
import {getTemplate} from './user_education_internals_card.html.js';

const PROMO_LAUNCH_EVENT = 'promo-launch';

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
    };
  }

  promo: FeaturePromoDemoPageInfo;

  private launchPromo_() {
    this.dispatchEvent(new CustomEvent(
        PROMO_LAUNCH_EVENT,
        {bubbles: true, composed: true, detail: this.promo.internalName}));
  }

  private showDescription_() {
    return this.promo.displayDescription !== '';
  }

  private formatDate_() {
    const date = new Date(Number(this.promo.addedTimestampMs));
    return date.toDateString();
  }

  private formatPlatforms_() {
    return this.promo.supportedPlatforms.join(', ');
  }

  private showInstructions_() {
    return this.promo.instructions.length;
  }
}

customElements.define(
    UserEducationInternalsCardElement.is, UserEducationInternalsCardElement);
