// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';
import {getCss} from './privacy_sandbox_privacy_policy_dialog.css.js';
import {getHtml} from './privacy_sandbox_privacy_policy_dialog.html.js';

export class PrivacySandboxPrivacyPolicyDialogElement extends CrLitElement {
  static get is() {
    return 'privacy-sandbox-privacy-policy-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shouldShow: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  shouldShow: boolean = false;
  private privacyPolicyPageClickStartTime_: number;
  private privacyPolicyPageLoadEndTime_: number;

  override firstUpdated() {
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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // Modified by one way binding from parent component after the user clicks
    // on the Privacy Policy Link.
    if (changedProperties.has('shouldShow') && this.shouldShow) {
      // Send focus on the first element (back button) for a11y screen reader.
      const backButton =
          this.shadowRoot!.querySelector<HTMLElement>('#backButton');
      assert(backButton);
      backButton.focus();

      this.privacyPolicyPageClickStartTime_ = performance.now();
      PrivacySandboxDialogBrowserProxy.getInstance().promptActionOccurred(
          PrivacySandboxPromptAction.PRIVACY_POLICY_LINK_CLICKED);
      // Tracks when the privacy policy page is loaded before the link is
      // clicked.
      if (this.privacyPolicyPageLoadEndTime_) {
        this.recordPrivacyPolicyLoadTime_(
            this.privacyPolicyPageLoadEndTime_ -
            this.privacyPolicyPageClickStartTime_);
      }
    }
  }

  private recordPrivacyPolicyLoadTime_(privacyPolicyLoadDuration: number) {
    PrivacySandboxDialogBrowserProxy.getInstance().recordPrivacyPolicyLoadTime(
        privacyPolicyLoadDuration);
  }

  protected onBackToConsentNotice_() {
    // Mark the privacy policy iframe and button to hidden.
    this.shouldShow = false;
    this.fire('back-button-clicked');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-privacy-policy-dialog':
        PrivacySandboxPrivacyPolicyDialogElement;
  }
}

customElements.define(
    PrivacySandboxPrivacyPolicyDialogElement.is,
    PrivacySandboxPrivacyPolicyDialogElement);
