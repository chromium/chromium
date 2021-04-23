// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This element contains a set of SVGs that together acts as an
 * animated and responsive background for any page that contains it.
 */
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './navi_colors_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {stepIndicatorModel} from './nux_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const StepIndicatorElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class StepIndicatorElement extends StepIndicatorElementBase {
  static get is() {
    return 'step-indicator';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {stepIndicatorModel} */
      model: Object,

      /** @private */
      dots_: {
        type: Array,
        computed: 'computeDots_(model.total)',
      },
    };
  }

  /**
   * @param {number} active
   * @param {number} total
   * @return {string}
   * @private
   */
  computeLabel_(active, total) {
    return this.i18n('stepsLabel', active + 1, total);
  }

  /**
   * @return {!Array<undefined>}
   * @private
   */
  computeDots_() {
    // If total is 1, show nothing.
    return new Array(this.model.total > 1 ? this.model.total : 0);
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getActiveClass_(index) {
    return index === this.model.active ? 'active' : '';
  }
}
customElements.define(StepIndicatorElement.is, StepIndicatorElement);
