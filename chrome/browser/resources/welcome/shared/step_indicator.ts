// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This element contains a set of SVGs that together acts as an
 * animated and responsive background for any page that contains it.
 */
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './navi_colors_css.js';

import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {stepIndicatorModel} from './nux_types.js';

const StepIndicatorElementBase = I18nMixin(PolymerElement);

/** @polymer */
export class StepIndicatorElement extends StepIndicatorElementBase {
  static get is() {
    return 'step-indicator';
  }

  static get properties() {
    return {
      model: Object,

      dots_: {
        type: Array,
        computed: 'computeDots_(model.total)',
      }
    };
  }

  model?: stepIndicatorModel;
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

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(StepIndicatorElement.is, StepIndicatorElement);
