// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './feature_showcase_step.css.js';
import {getHtml} from './feature_showcase_step.html.js';

export class FeatureShowcaseStepElement extends CrLitElement {
  static get is() {
    return 'feature-showcase-step';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-step': FeatureShowcaseStepElement;
  }
}

customElements.define(
    FeatureShowcaseStepElement.is, FeatureShowcaseStepElement);
