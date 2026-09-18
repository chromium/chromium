// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'step-indicator' is an element that displays a row of dots, one of which is
 * highlighted, to indicate how far the user is through a multi-step flow.
 */
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './step_indicator.css.js';
import {getHtml} from './step_indicator.html.js';

const StepIndicatorElementBase = I18nMixinLit(CrLitElement);

export interface StepIndicatorModel {
  active: number;
  total: number;
}

export class StepIndicatorElement extends StepIndicatorElementBase {
  static get is() {
    return 'step-indicator';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * An Object with 'active' and 'total' members, indicating the active dot
       * index and the total number of dots.
       */
      model: {type: Object},

      /**
       * An array with length equal to the number of dots, for use by
       * dom-repeat. The contents of the array are unused.
       */
      dots_: {type: Array},
    };
  }

  accessor model: StepIndicatorModel|undefined;
  protected accessor dots_: void[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('model')) {
      this.dots_ = this.computeDots_();
    }
  }

  /** @return the screenreader label for this element. */
  protected computeA11yLabel_(): string {
    if (!this.model) {
      return '';
    }
    return this.i18n(
        'privacyGuideSteps', this.model.active + 1, this.model.total);
  }

  private computeDots_(): void[] {
    // If total is 1, show nothing.
    return new Array(this.model && this.model.total > 1 ? this.model.total : 0);
  }

  /**
   * Returns a class for the dot at `index`, which will highlight the dot at the
   * active index.
   */
  protected getActiveClass_(index: number): string {
    return index === this.model?.active ? 'active' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'step-indicator': StepIndicatorElement;
  }
}

customElements.define(StepIndicatorElement.is, StepIndicatorElement);
