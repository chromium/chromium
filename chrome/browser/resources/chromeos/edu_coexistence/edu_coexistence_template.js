// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from 'chrome://resources/ash/common/cr_scrollable_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 */
const EduCoexistenceTemplateBase =
    mixinBehaviors([CrScrollableBehavior], PolymerElement);

/**
 * @polymer
 */
class EduCoexistenceTemplate extends EduCoexistenceTemplateBase {
  static get is() {
    return 'edu-coexistence-template';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Indicates whether the footer/button div should be shown.
       * @private
       */
      showButtonFooter_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * Shows/hides the button footer.
   * @param {boolean} show Whether to show the footer.
   */
  showButtonFooter(show) {
    this.showButtonFooter_ = show;
  }
}

customElements.define(EduCoexistenceTemplate.is, EduCoexistenceTemplate);
