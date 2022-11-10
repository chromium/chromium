// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_combined_dialog_app.html.js';

export enum PrivacySandboxCombinedDialogStep {
  CONSENT = 'consent',
  SAVING = 'saving',
  NOTICE = 'notice',
}

export class PrivacySandboxCombinedDialogAppElement extends PolymerElement {
  static get is() {
    return 'privacy-sandbox-combined-dialog-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      step_: String,
      stepEnum_: {
        type: Object,
        value: PrivacySandboxCombinedDialogStep,
      },
    };
  }

  private step_: string;

  override ready() {
    super.ready();

    // Support starting with notice step instead of starting with consent step.
    const step = new URLSearchParams(window.location.search).get('step');
    if (step === PrivacySandboxCombinedDialogStep.NOTICE) {
      this.step_ = step;
    } else {
      this.step_ = PrivacySandboxCombinedDialogStep.CONSENT;
    }
  }

  private isCurrentStep_(step: PrivacySandboxCombinedDialogStep) {
    return this.step_ === step;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-combined-dialog-app':
        PrivacySandboxCombinedDialogAppElement;
  }
}

customElements.define(
    PrivacySandboxCombinedDialogAppElement.is,
    PrivacySandboxCombinedDialogAppElement);
