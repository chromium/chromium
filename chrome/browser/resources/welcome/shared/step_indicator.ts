// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This element contains a set of SVGs that together acts as an
 * animated and responsive background for any page that contains it.
 */
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './navi_colors.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {StepIndicatorModel} from './nux_types.js';
import {getTemplate} from './step_indicator.html.js';

const StepIndicatorElementBase = I18nMixin(PolymerElement);

/** @polymer */
export class StepIndicatorElement extends StepIndicatorElementBase {
  static get is() {
    return 'step-indicator';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,

      dots_: {
        type: Array,
        computed: 'computeDots_(model.total)',
      },
    };
  }

  model?: StepIndicatorModel;
  private dots_?: undefined[];

  private computeLabel_(active: number, total: number): string {
    return this.i18n('stepsLabel', active + 1, total);
  }

  private computeDots_(): undefined[] {
    // If total is 1, show nothing.
    return new Array(this.model!.total > 1 ? this.model!.total : 0);
  }

  private getActiveClass_(index: number): string {
    return index === this.model!.active ? 'active' : '';
  }
}
customElements.define(StepIndicatorElement.is, StepIndicatorElement);
