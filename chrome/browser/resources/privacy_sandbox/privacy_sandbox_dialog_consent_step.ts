// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';
import './shared_style.css.js';
import './privacy_sandbox_dialog_learn_more.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_dialog_consent_step.html.js';

export class PrivacySandboxDialogConsentStepElement extends PolymerElement {
  static get is() {
    return 'privacy-sandbox-dialog-consent-step';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expanded_: {
        type: Boolean,
        observer: 'onLearnMoreExpandedChanged_',
      },
    };
  }

  private onLearnMoreExpandedChanged_(newValue: boolean, oldValue: boolean) {
    // Check both old and new value to avoid reporting actions when the dialog
    // just was created and oldValue is undefined.
    if (newValue && !oldValue) {
      this.promptActionOccurred(
          PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    }
    if (!newValue && oldValue) {
      this.promptActionOccurred(
          PrivacySandboxPromptAction.CONSENT_MORE_INFO_CLOSED);
    }
  }

  private onConsentAccepted_() {
    // TODO(crbug.com/1378703): Handle user action.
    this.dispatchEvent(
        new CustomEvent('consent-resolved', {bubbles: true, composed: true}));
  }

  private onConsentDeclined_() {
    // TODO(crbug.com/1378703): Handle user action.
    this.dispatchEvent(
        new CustomEvent('consent-resolved', {bubbles: true, composed: true}));
  }

  private promptActionOccurred(action: PrivacySandboxPromptAction) {
    PrivacySandboxDialogBrowserProxy.getInstance().promptActionOccurred(action);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-dialog-consent-step':
        PrivacySandboxDialogConsentStepElement;
  }
}

customElements.define(
    PrivacySandboxDialogConsentStepElement.is,
    PrivacySandboxDialogConsentStepElement);
