// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '../feature_showcase_step.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './example_step.css.js';
import {getHtml} from './example_step.html.js';

export class FeatureShowcaseExampleStepElement extends CrLitElement {
  static get is() {
    return 'feature-showcase-example-step';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onButtonClick_() {
    this.fire('step-completed');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-example-step': FeatureShowcaseExampleStepElement;
  }
}

customElements.define(
    FeatureShowcaseExampleStepElement.is, FeatureShowcaseExampleStepElement);
