// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '../feature_showcase_step.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './default_browser_step.css.js';
import {getHtml} from './default_browser_step.html.js';

export class FeatureShowcaseDefaultBrowserStepElement extends CrLitElement {
  static get is() {
    return 'feature-showcase-default-browser-step';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onConfirmButtonClick_() {
    // TODO(b/505629973): Add sending the event to the browser.
    this.fire('step-completed');
  }

  protected onSkipButtonClick_() {
    // TODO(b/505629973): Add sending the event to the browser.
    this.fire('step-completed');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-default-browser-step':
        FeatureShowcaseDefaultBrowserStepElement;
  }
}

customElements.define(
    FeatureShowcaseDefaultBrowserStepElement.is,
    FeatureShowcaseDefaultBrowserStepElement);
