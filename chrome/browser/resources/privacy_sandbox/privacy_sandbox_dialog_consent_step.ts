// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './strings.m.js';
import './shared_style.css.js';
import './privacy_sandbox_dialog_learn_more.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_dialog_consent_step.html.js';
import {PrivacySandboxDialogMixin} from './privacy_sandbox_dialog_mixin.js';

const PrivacySandboxDialogConsentStepElementBase =
    PrivacySandboxDialogMixin(PolymerElement);

export class PrivacySandboxDialogConsentStepElement extends
    PrivacySandboxDialogConsentStepElementBase {
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
        observer: 'onConsentLearnMoreExpandedChanged',
      },
      isPrivacySandboxPrivacyPolicyEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isPrivacySandboxPrivacyPolicyEnabled'),
      },
    };
  }

  private privacyPolicyPageClickStartTime_: number;
  private privacyPolicyPageLoadEndTime_: number;
  private isPrivacySandboxPrivacyPolicyEnabled_: boolean;

  override ready() {
    super.ready();

    // Hide pre-loading the privacy policy page behind a flag.
    if (this.isPrivacySandboxPrivacyPolicyEnabled_) {
      const button = document.createElement('cr-icon-button');
      button.id = 'backButton';
      button.className = 'icon-arrow-back back-button hidden';
      button.onclick = () => {
        this.onBackToNotice_();
      };

      const iframe = document.createElement('iframe');
      iframe.id = 'privacyPolicy';
      iframe.className = 'iframe hidden';
      iframe.tabIndex = -1;
      iframe.setAttribute('frameBorder', '0');

      this.shadowRoot!.querySelector<HTMLElement>(
                          '.iframe-container')!.appendChild(button);
      this.shadowRoot!.querySelector<HTMLElement>(
                          '.iframe-container')!.appendChild(iframe);
    }

    window.addEventListener('message', event => {
      if (event.data.id === 'privacy-policy-loaded') {
        this.privacyPolicyPageLoadEndTime_ = event.data.value;
        // Tracks when the privacy policy page is loaded after the link is
        // clicked.
        if (this.privacyPolicyPageClickStartTime_) {
          this.recordPrivacyPolicyLoadTime_(
              this.privacyPolicyPageLoadEndTime_ -
              this.privacyPolicyPageClickStartTime_);
        }
        return;
      }
    });
  }

  private onConsentAccepted_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.CONSENT_ACCEPTED);
    this.dispatchEvent(
        new CustomEvent('consent-resolved', {bubbles: true, composed: true}));
  }

  private onConsentDeclined_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.CONSENT_DECLINED);
    this.dispatchEvent(
        new CustomEvent('consent-resolved', {bubbles: true, composed: true}));
  }

  private recordPrivacyPolicyLoadTime_(privacyPolicyLoadDuration: number) {
    PrivacySandboxDialogBrowserProxy.getInstance().recordPrivacyPolicyLoadTime(
        privacyPolicyLoadDuration);
  }

  private onPrivacyPolicyLinkClicked_() {
    this.privacyPolicyPageClickStartTime_ = performance.now();
    this.promptActionOccurred(
        PrivacySandboxPromptAction.PRIVACY_POLICY_LINK_CLICKED);
    // Tracks when the privacy policy page is loaded before the link is clicked.
    if (this.privacyPolicyPageLoadEndTime_) {
      this.recordPrivacyPolicyLoadTime_(
          this.privacyPolicyPageLoadEndTime_ -
          this.privacyPolicyPageClickStartTime_);
    }
    // Move the consent notice to the back.
    this.shadowRoot!.querySelector<HTMLElement>(
                        '#consentNotice')!.style.display = 'none';

    // Move the privacy policy iframe to the front.
    const iframeContent =
        this.shadowRoot!.querySelector<HTMLElement>('#privacyPolicy');
    iframeContent!.classList.add('visible');
    iframeContent!.classList.remove('hidden');

    // Move the back button to the front.
    const backButton =
        this.shadowRoot!.querySelector<HTMLElement>('#backButton');
    backButton!.classList.add('visible');
    backButton!.classList.remove('hidden');
  }

  private onBackToNotice_() {
    // Move the privacy policy iframe to the back.
    const iframeContent =
        this.shadowRoot!.querySelector<HTMLElement>('#privacyPolicy');
    iframeContent!.classList.add('hidden');
    iframeContent!.classList.remove('visible');

    // Move the back button to the back.
    const backButton =
        this.shadowRoot!.querySelector<HTMLElement>('#backButton');
    backButton!.classList.add('hidden');
    backButton!.classList.remove('visible');

    // Move the consent notice to the front.
    this.shadowRoot!.querySelector<HTMLElement>(
                        '#consentNotice')!.style.display = 'inline-block';
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
