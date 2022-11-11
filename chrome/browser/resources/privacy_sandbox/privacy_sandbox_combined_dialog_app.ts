// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './shared_style.css.js';

import {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_combined_dialog_app.html.js';
import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
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

  override ready() {
    super.ready();

    // Support starting with notice step instead of starting with consent step.
    const step = new URLSearchParams(window.location.search).get('step');
    let promise: Promise<void>;
    if (step === PrivacySandboxCombinedDialogStep.NOTICE) {
      promise = this.navigateToStep_(PrivacySandboxCombinedDialogStep.NOTICE);
    } else {
      promise = this.navigateToStep_(PrivacySandboxCombinedDialogStep.CONSENT);
    }
    // After the initial step was loaded, resize the native dialog to fit it..
    promise.then(() => this.resizeNativeDialog());
  }

  private onConsentStepResolved_() {
    const savingDurationMs = 1500;
    this.navigateToStep_(PrivacySandboxCombinedDialogStep.SAVING)
        .then(
            () => new Promise(r => setTimeout(r, savingDurationMs))
                      .then(
                          () => this.navigateToStep_(
                              PrivacySandboxCombinedDialogStep.NOTICE)));
  }

  private navigateToStep_(step: PrivacySandboxCombinedDialogStep):
      Promise<void> {
    assert(step !== this.step_);
    this.step_ = step;
    return this.$.viewManager.switchView(this.step_);
  }

  private onNoticeOpenSettings_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  }

  private onNoticeAcknowledge_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  }

  private promptActionOccurred(action: PrivacySandboxPromptAction) {
    PrivacySandboxDialogBrowserProxy.getInstance().promptActionOccurred(action);
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
