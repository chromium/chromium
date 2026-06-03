// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './feature_showcase_stepper.css.js';
import {getHtml} from './feature_showcase_stepper.html.js';

export class FeatureShowcaseStepperElement extends CrLitElement {
  static get is() {
    return 'feature-showcase-stepper';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      steps: {type: Array},
      activeIndex: {type: Number},
    };
  }

  accessor steps: string[] = [];
  accessor activeIndex: number = 0;
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-stepper': FeatureShowcaseStepperElement;
  }
}

customElements.define(
    FeatureShowcaseStepperElement.is, FeatureShowcaseStepperElement);
