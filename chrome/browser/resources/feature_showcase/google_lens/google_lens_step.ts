// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../feature_showcase_step.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {GoogleLensBrowserProxyImpl} from './google_lens_browser_proxy.js';
import {getCss} from './google_lens_step.css.js';
import {getHtml} from './google_lens_step.html.js';

export class FeatureShowcaseGoogleLensStepElement extends CrLitElement {
  static get is() {
    return 'feature-showcase-google-lens-step';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      buttonsDisabled: {type: Boolean},
    };
  }

  accessor buttonsDisabled: boolean = false;

  protected onConfirmClick_() {
    this.buttonsDisabled = true;
    GoogleLensBrowserProxyImpl.getInstance().handler.enableGoogleLens();
    this.fire('step-completed');
  }

  protected onSkipClick_() {
    this.buttonsDisabled = true;
    GoogleLensBrowserProxyImpl.getInstance().handler.skipGoogleLens();
    this.fire('step-completed');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-google-lens-step': FeatureShowcaseGoogleLensStepElement;
  }
}

customElements.define(
    FeatureShowcaseGoogleLensStepElement.is,
    FeatureShowcaseGoogleLensStepElement);
