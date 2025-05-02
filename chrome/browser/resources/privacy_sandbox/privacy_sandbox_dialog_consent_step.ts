// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '/strings.m.js';
import './shared_style.css.js';
import './privacy_sandbox_dialog_learn_more.js';
import './privacy_sandbox_privacy_policy_dialog.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
import {getTemplate} from './privacy_sandbox_dialog_consent_step.html.js';
import {PrivacySandboxDialogMixin} from './privacy_sandbox_dialog_mixin.js';

const PrivacySandboxDialogConsentStepElementBase =
    PrivacySandboxDialogMixin(I18nMixin(PolymerElement));

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
       * If true, the consent notice page is hidden.
       * On load, this page should not be hidden.
       */
      hideConsentNoticePage_: {
        type: Boolean,
        value: false,
      },

      /**
       * If true, the Ad Topics Content parity should be shown.
       */
      shouldShowAdTopicsContentParity_: {
        type: Boolean,
        value: false,
      },

      consentContentV2FirstDescription_: {
        type: String,
        computed:
            'computeConsentContentV2FirstDescription_(shouldShowAdTopicsContentParity_)',
      },
    };
  }

  declare private expanded_: boolean;
  declare private hideConsentNoticePage_: boolean;
  declare private shouldShowAdTopicsContentParity_: boolean;
  declare private consentContentV2FirstDescription_: string;

  override ready() {
    super.ready();

    PrivacySandboxDialogBrowserProxy.getInstance()
        .shouldShowAdTopicsContentParity()
        .then(shouldShow => {
          this.shouldShowAdTopicsContentParity_ = shouldShow;
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
    this.loadPrivacyPolicyOnExpand(newValue, oldValue);
    this.onConsentLearnMoreExpandedChanged(newValue, oldValue);
  }

  private onBackButtonClicked_() {
    this.hideConsentNoticePage_ = false;
    const privacyPolicyLinkId = this.shouldShowV2() ?
        (this.shouldShowAdTopicsContentParity_ ? '#privacyPolicyLinkV3' :
                                                 '#privacyPolicyLinkV2') :
        '#privacyPolicyLink';
    // Send focus back to privacy policy link for a11y screen reader.
    this.shadowRoot!.querySelector<HTMLElement>(privacyPolicyLinkId)!.focus();
  }

  private onPrivacyPolicyLinkClicked_() {
    this.hideConsentNoticePage_ = true;
  }

  private computeConsentContentV2FirstDescription_(): string {
    return this.i18n(
        this.shouldShowAdTopicsContentParity_ ?
            'm1ConsentDescription1ContentParity' :
            'm1ConsentDescription2V2');
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
