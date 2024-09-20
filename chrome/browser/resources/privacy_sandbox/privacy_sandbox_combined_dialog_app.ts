// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './strings.m.js';
import './shared_style.css.js';
import './privacy_sandbox_dialog_consent_step.js';
import './privacy_sandbox_dialog_notice_step.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_combined_dialog_app.html.js';
import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
import type {PrivacySandboxDialogMixinInterface} from './privacy_sandbox_dialog_mixin.js';
import {PrivacySandboxDialogResizeMixin} from './privacy_sandbox_dialog_resize_mixin.js';

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

type PrivacySandboxStepElement = PrivacySandboxDialogMixinInterface&HTMLElement;

const PrivacySandboxCombinedDialogAppElementBase =
    PrivacySandboxDialogResizeMixin(PolymerElement);

export class PrivacySandboxCombinedDialogAppElement extends
    PrivacySandboxCombinedDialogAppElementBase {
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
  private animationsEnabled_: boolean = true;

  override ready() {
    super.ready();

    // Support starting with notice step instead of starting with consent step.
    const step = new URLSearchParams(window.location.search).get('step');
    const startWithNotice = step === PrivacySandboxCombinedDialogStep.NOTICE;

    const firstStep = startWithNotice ?
        PrivacySandboxCombinedDialogStep.NOTICE :
        PrivacySandboxCombinedDialogStep.CONSENT;
    // After the initial step was loaded, resize the native dialog to fit it.
    this.navigateToStep_(firstStep)
        .then(() => this.resizeAndShowNativeDialog())
        .then(() => this.updateScrollableContentsCurrentStep_())
        .then(
            () => this.promptActionOccurred(
                startWithNotice ? PrivacySandboxPromptAction.NOTICE_SHOWN :
                                  PrivacySandboxPromptAction.CONSENT_SHOWN));
  }

  disableAnimationsForTesting() {
    this.animationsEnabled_ = false;
  }

  private onConsentStepResolved_() {
    const savingDurationMs = this.animationsEnabled_ ? 1500 : 0;
    this.navigateToStep_(PrivacySandboxCombinedDialogStep.SAVING)
        .then(() => new Promise(r => setTimeout(r, savingDurationMs)))
        .then(() => {
          this.navigateToStep_(PrivacySandboxCombinedDialogStep.NOTICE);
          this.updateScrollableContentsCurrentStep_().then(
              () => this.promptActionOccurred(
                  PrivacySandboxPromptAction.NOTICE_SHOWN));
        });
  }

  private navigateToStep_(step: PrivacySandboxCombinedDialogStep):
      Promise<void> {
    assert(step !== this.step_);
    this.step_ = step;
    const enterAnimation = this.animationsEnabled_ ? 'fade-in' : 'no-animation';
    const exitAnimation = this.animationsEnabled_ ? 'fade-out' : 'no-animation';
    return this.$.viewManager.switchView(
        this.step_, enterAnimation, exitAnimation);
  }

  private promptActionOccurred(action: PrivacySandboxPromptAction) {
    PrivacySandboxDialogBrowserProxy.getInstance().promptActionOccurred(action);
  }

  private updateScrollableContentsCurrentStep_(): Promise<void> {
    // After the dialog was resized and filled content, trigger
    // `updateScrollableContents()` and `maybeShowMoreButton` for the current
    // step (consent or notice).
    const stepElement = this.getStepElement_(this.step_);
    stepElement.updateScrollableContents();
    return stepElement.maybeShowMoreButton();
  }

  private getStepElement_(step: PrivacySandboxCombinedDialogStep):
      PrivacySandboxStepElement {
    return this.shadowRoot!.querySelector<PrivacySandboxStepElement>(
        `#${step}`)!;
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
