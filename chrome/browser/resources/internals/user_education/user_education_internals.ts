// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './user_education_internals.html.js';
import {FeaturePromoDemoPageInfo, UserEducationInternalsPageHandler, UserEducationInternalsPageHandlerInterface} from './user_education_internals.mojom-webui.js';

class UserEducationInternalsElement extends PolymerElement {
  static get is() {
    return 'user-education-internals';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of tutorials and feature_promos that can be started.
       * Each tutorial has a string identifier.
       */
      tutorials_: Array,
      featurePromos_: Array,
      featurePromoErrorMessage_: String,
    };
  }

  private tutorials_: string[];
  private featurePromos_: FeaturePromoDemoPageInfo[];
  private featurePromoErrorMessage_: string;

  private handler_: UserEducationInternalsPageHandlerInterface;

  constructor() {
    super();
    this.handler_ = UserEducationInternalsPageHandler.getRemote();
  }

  override ready() {
    super.ready();

    this.handler_.getTutorials().then(({tutorialIds}) => {
      this.tutorials_ = tutorialIds;
    });

    this.handler_.getFeaturePromos().then(({featurePromos}) => {
      this.featurePromos_ = featurePromos;
    });
  }

  private startTutorial_(e: DomRepeatEvent<string>) {
    const id = e.model.item;
    this.handler_.startTutorial(id);
  }

  private showFeaturePromo_(e: DomRepeatEvent<FeaturePromoDemoPageInfo>) {
    const id = e.model.item.displayTitle;
    this.featurePromoErrorMessage_ = '';

    this.handler_.showFeaturePromo(id).then(({errorMessage}) => {
      this.featurePromoErrorMessage_ = errorMessage;
    });
  }
}

customElements.define(
    UserEducationInternalsElement.is, UserEducationInternalsElement);
