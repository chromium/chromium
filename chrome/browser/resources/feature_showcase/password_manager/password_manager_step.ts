// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../feature_showcase_step.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerBrowserProxyImpl} from './password_manager_browser_proxy.js';
import {getCss} from './password_manager_step.css.js';
import {getHtml} from './password_manager_step.html.js';

export class FeatureShowcasePasswordManagerStepElement extends CrLitElement {
  static get is() {
    return 'feature-showcase-password-manager-step';
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
    PasswordManagerBrowserProxyImpl.getInstance().handler.pinPasswordManager();
    this.fire('step-completed');
  }

  protected onSkipClick_() {
    // TODO(crbug.com/505631006): Add unified way to handle "No, thanks".
    this.fire('step-completed');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feature-showcase-password-manager-step':
        FeatureShowcasePasswordManagerStepElement;
  }
}

customElements.define(
    FeatureShowcasePasswordManagerStepElement.is,
    FeatureShowcasePasswordManagerStepElement);
