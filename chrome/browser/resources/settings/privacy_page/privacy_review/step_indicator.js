// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'step-indicator' is an element that displays a row of dots, one of which is
 * highlighted, to indicate how far the user is through a multi-step flow.
 */
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const StepIndicatorBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class StepIndicator extends StepIndicatorBase {
  static get is() {
    return 'step-indicator';
  }

  static get properties() {
    return {
      /**
       * An Object with 'active' and 'total' members, indicating the active dot
       * index and the total number of dots.
       */
      model: Object,

      /**
       * An array with length equal to the number of dots, for use by
       * dom-repeat. The contents of the array are unused.
       * @private {Array<undefined>}
       */
      dots_: {
        type: Array,
        computed: 'computeDots_(model.total)',
      }
    };
  }

  /**
   * Returns the screenreader label for this element.
   * @private
   * @return {string}
   */
  computeA11yLabel_() {
    return this.i18n(
        'privacyReviewSteps', this.model.active + 1, this.model.total);
  }

  /**
   * @private
   * @return {Array<undefined>}
   */
  computeDots_() {
    // If total is 1, show nothing.
    return new Array(this.model.total > 1 ? this.model.total : 0);
  }

  /**
   * Returns a class for the dot at `index`, which will highlight the dot at the
   * active index.
   * @private
   * @return {string}
   */
  getActiveClass_(index) {
    return index === this.model.active ? 'active' : '';
  }


  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(StepIndicator.is, StepIndicator);
