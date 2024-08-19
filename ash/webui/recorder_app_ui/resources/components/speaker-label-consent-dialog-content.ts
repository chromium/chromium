// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css, html, LitElement} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';

/**
 * The content of the speaker label consent dialog.
 *
 * This is reused between speaker-label-consent-dialog and onboarding-dialog.
 */
export class SpeakerLabelConsentDialogContent extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }

    ul {
      padding-inline-start: 20px;
    }

    a,
    a:visited {
      color: var(--cros-sys-primary);
    }
  `;

  override render(): RenderResult {
    // TODO: b/336963138 - Add correct link
    return html`
      ${i18n.onboardingDialogSpeakerLabelDescriptionPrefix}
      <ul>
        <li>${i18n.onboardingDialogSpeakerLabelDescriptionListItem1}</li>
        <li>${i18n.onboardingDialogSpeakerLabelDescriptionListItem2}</li>
        <li>${i18n.onboardingDialogSpeakerLabelDescriptionListItem3}</li>
      </ul>
      ${i18n.onboardingDialogSpeakerLabelDescriptionSuffix}
      <a href="javascript:;">
        ${i18n.onboardingDialogSpeakerLabelLearnMoreLink}
      </a>
    `;
  }
}

window.customElements.define(
  'speaker-label-consent-dialog-content',
  SpeakerLabelConsentDialogContent,
);

declare global {
  interface HTMLElementTagNameMap {
    'speaker-label-consent-dialog-content': SpeakerLabelConsentDialogContent;
  }
}
