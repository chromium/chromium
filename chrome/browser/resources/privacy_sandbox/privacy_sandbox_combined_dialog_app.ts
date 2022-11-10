// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './strings.m.js';

import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_combined_dialog_app.html.js';

export enum PrivacySandboxCombinedDialogStep {
  CONSENT = 'consent',
  SAVING = 'saving',
  NOTICE = 'notice',
}

export interface PrivacySandboxCombinedDialogAppElement {
  $: {
    viewManager: CrViewManagerElement,
  };
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

  private step_: PrivacySandboxCombinedDialogStep;

  override ready() {
    super.ready();

    // Support starting with notice step instead of starting with consent step.
    const step = new URLSearchParams(window.location.search).get('step');
    if (step === PrivacySandboxCombinedDialogStep.NOTICE) {
      this.navigateToStep_(PrivacySandboxCombinedDialogStep.NOTICE);
    } else {
      this.navigateToStep_(PrivacySandboxCombinedDialogStep.CONSENT);
    }
  }

  private onConsentStepResolved_() {
    this.navigateToStep_(PrivacySandboxCombinedDialogStep.SAVING);
    const savingDurationMs = 1000;
    setTimeout(() => {
      this.navigateToStep_(PrivacySandboxCombinedDialogStep.NOTICE);
    }, savingDurationMs);
  }

  private navigateToStep_(step: PrivacySandboxCombinedDialogStep) {
    assert(step !== this.step_);
    this.step_ = step;

    // TODO(crbug.com/1378703): Check if animations are disabled by global
    // control.
    const animateFromLeftToRight =
        loadTimeData.getString('textdirection') === 'ltr';
    this.$.viewManager.switchView(
        this.step_,
        animateFromLeftToRight ? 'slide-in-fade-in-ltr' :
                                 'slide-in-fade-in-rtl');
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
