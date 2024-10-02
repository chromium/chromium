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
        observer: 'onConsentLearnMoreExpanded_',
      },

      /**
       * If true, the privacy policy text is hyperlinked.
       */
      isPrivacyPolicyLinkEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * If true, the consent notice page is hidden.
       * On load, this page should not be hidden.
       */
      hideConsentNoticePage_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private privacyPolicyPageClickStartTime_: number;
  private privacyPolicyPageLoadEndTime_: number;
  private isPrivacyPolicyLinkEnabled_: boolean;
  private hideConsentNoticePage_: boolean;

  override ready() {
    super.ready();

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

  private onConsentLearnMoreExpanded_(newValue: boolean, oldValue: boolean) {
    this.loadPrivacyPolicyOnExpand_(newValue, oldValue);
    this.onConsentLearnMoreExpandedChanged(newValue, oldValue);
  }

  private loadPrivacyPolicyOnExpand_(newValue: boolean, oldValue: boolean) {
    // When the expand is triggered, if the iframe hasn't been loaded yet,
    // load it the first time the learn more expand section is clicked.
    if (newValue && !oldValue) {
      if (!this.shadowRoot!.querySelector<HTMLIFrameElement>(
              '#privacyPolicy')) {
        PrivacySandboxDialogBrowserProxy.getInstance()
            .shouldShowPrivacySandboxPrivacyPolicy()
            .then(isPrivacyPolicyLinkEnabled => {
              this.isPrivacyPolicyLinkEnabled_ = isPrivacyPolicyLinkEnabled;
            });
      }
    }
  }

  private recordPrivacyPolicyLoadTime_(privacyPolicyLoadDuration: number) {
    PrivacySandboxDialogBrowserProxy.getInstance().recordPrivacyPolicyLoadTime(
        privacyPolicyLoadDuration);
  }

  private onBackToConsentNotice_() {
    // Move the privacy policy iframe to the back.
    const iframeContent =
        this.shadowRoot!.querySelector<HTMLElement>('#privacyPolicy');
    iframeContent!.classList.add('hidden');
    iframeContent!.classList.remove('visible');
    this.hideConsentNoticePage_ = false;
  }

  private onPrivacyPolicyLinkClicked_() {
    // Move the privacy policy iframe to the front.
    // By manually setting the visibility, the privacy policy page
    // is able to preload while staying hidden.
    const iframeContent =
        this.shadowRoot!.querySelector<HTMLElement>('#privacyPolicy');
    iframeContent!.classList.add('visible');
    iframeContent!.classList.remove('hidden');

    this.hideConsentNoticePage_ = true;
    this.privacyPolicyPageClickStartTime_ = performance.now();
    this.promptActionOccurred(
        PrivacySandboxPromptAction.PRIVACY_POLICY_LINK_CLICKED);
    // Tracks when the privacy policy page is loaded before the link is clicked.
    if (this.privacyPolicyPageLoadEndTime_) {
      this.recordPrivacyPolicyLoadTime_(
          this.privacyPolicyPageLoadEndTime_ -
          this.privacyPolicyPageClickStartTime_);
    }
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
