// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This element contains a set of SVGs that together acts as an
 * animated and responsive background for any page that contains it.
 */

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {StepIndicatorModel} from './nux_types.js';
import {getCss} from './step_indicator.css.js';
import {getHtml} from './step_indicator.html.js';

const StepIndicatorElementBase = I18nMixinLit(CrLitElement);

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
      model: {type: Object},
      dots_: {type: Array},
    };
  }

  model?: StepIndicatorModel;
  protected dots_: number[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('model')) {
      this.dots_ = this.computeDots_();
    }
  }

  protected getLabel_(): string {
    return this.model ?
        this.i18n('stepsLabel', this.model!.active + 1, this.model!.total) :
        '';
  }

  private computeDots_(): number[] {
    assert(this.model);
    // If total is 1, show nothing.
    const array: number[] = new Array(this.model.total > 1 ? this.model.total : 0);
    array.fill(0);
    return array;
  }

  protected getActiveClass_(index: number): string {
    assert(this.model);
    return index === this.model.active ? 'active' : '';
  }
}

customElements.define(StepIndicatorElement.is, StepIndicatorElement);
