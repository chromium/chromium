// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css, html, LitElement} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';

/**
 * The content of the speaker ID consent dialog.
 *
 * This is reused between speaker-id-consent-dialog and onboarding-dialog.
 */
export class SpeakerIdConsentDialogContent extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }

    ul {
      padding-inline-start: 20px;
    }
  `;

  override render(): RenderResult {
    // TODO: b/336963138 - Add correct link
    return html`
      ${i18n.onboardingDialogSpeakerIdDescriptionPrefix}
      <ul>
        <li>${i18n.onboardingDialogSpeakerIdDescriptionListItem1}</li>
        <li>${i18n.onboardingDialogSpeakerIdDescriptionListItem2}</li>
        <li>${i18n.onboardingDialogSpeakerIdDescriptionListItem3}</li>
      </ul>
      ${i18n.onboardingDialogSpeakerIdDescriptionSuffix}
      <a href="javascript:;">${i18n.onboardingDialogSpeakerIdLearnMoreLink}</a>
    `;
  }
}

window.customElements.define(
  'speaker-id-consent-dialog-content',
  SpeakerIdConsentDialogContent,
);

declare global {
  interface HTMLElementTagNameMap {
    'speaker-id-consent-dialog-content': SpeakerIdConsentDialogContent;
  }
}
