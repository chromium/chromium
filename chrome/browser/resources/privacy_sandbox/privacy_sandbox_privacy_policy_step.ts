// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';
import './shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_privacy_policy_step.html.js';

export class PrivacySandboxPrivacyPolicyStepElement extends PolymerElement {
  static get is() {
    return 'privacy-sandbox-privacy-policy-step';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-privacy-policy-step':
        PrivacySandboxPrivacyPolicyStepElement;
  }
}

customElements.define(
    PrivacySandboxPrivacyPolicyStepElement.is,
    PrivacySandboxPrivacyPolicyStepElement);
