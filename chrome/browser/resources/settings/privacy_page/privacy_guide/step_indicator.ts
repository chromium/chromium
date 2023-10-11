// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'step-indicator' is an element that displays a row of dots, one of which is
 * highlighted, to indicate how far the user is through a multi-step flow.
 */
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './step_indicator.html.js';

const StepIndicatorBase = I18nMixin(PolymerElement);

export interface StepIndicatorModel {
  active: number;
  total: number;
}

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
       */
      dots_: {
        type: Array,
        computed: 'computeDots_(model.total)',
      },
    };
  }

  model: StepIndicatorModel;
  private dots_: void[];

  /**
   * @return the screenreader label for this element.
   */
  private computeA11yLabel_(): string {
    return this.i18n(
        'privacyGuideSteps', this.model.active + 1, this.model.total);
  }

  private computeDots_(): void[] {
    // If total is 1, show nothing.
    return new Array(this.model.total > 1 ? this.model.total : 0);
  }

  /**
   * Returns a class for the dot at `index`, which will highlight the dot at the
   * active index.
   */
  private getActiveClass_(index: number): string {
    return index === this.model.active ? 'active' : '';
  }


  static get template() {
    return getTemplate();
  }
}

customElements.define(StepIndicator.is, StepIndicator);
