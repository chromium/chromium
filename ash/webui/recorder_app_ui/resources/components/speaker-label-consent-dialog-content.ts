// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css, html, LitElement} from 'chrome://resources/mwc/lit/index.js';

import {i18n, NoArgStringName} from '../core/i18n.js';
import {HELP_URL} from '../core/url_constants.js';

// Since all string names need to be reported when recording user consent for
// speaker label, we declare separate constants here and use it for both the
// exported array and in the UI, to minimize the chance that those two got out
// of sync.
// Note that all of these (and using these as key to i18n) are still type
// checked with the NoArgStringName, and the consent recording method also only
// accepts arguments of the NoArgStringName type.
const DESCRIPTION_PREFIX_NAME: NoArgStringName =
  'onboardingDialogSpeakerLabelDescriptionPrefix';
const DESCRIPTION_LIST_ITEM_NAMES: NoArgStringName[] = [
  'onboardingDialogSpeakerLabelDescriptionListItem1',
  'onboardingDialogSpeakerLabelDescriptionListItem2',
  'onboardingDialogSpeakerLabelDescriptionListItem3',
];
const DESCRIPTION_SUFFIX_NAME: NoArgStringName =
  'onboardingDialogSpeakerLabelDescriptionSuffix';
const LINK_NAME: NoArgStringName = 'onboardingDialogSpeakerLabelLearnMoreLink';

export const DESCRIPTION_NAMES: NoArgStringName[] = [
  DESCRIPTION_PREFIX_NAME,
  ...DESCRIPTION_LIST_ITEM_NAMES,
  DESCRIPTION_SUFFIX_NAME,
  LINK_NAME,
];

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

  // All description in this component MUST be exported in the
  // `DESCRIPTION_NAMES` for consent auditing purpose.
  override render(): RenderResult {
    const listItems = DESCRIPTION_LIST_ITEM_NAMES.map(
      (name) => html`<li>${i18n[name]}</li>`,
    );
    return html`
      ${i18n[DESCRIPTION_PREFIX_NAME]}
      <ul>
        ${listItems}
      </ul>
      ${i18n[DESCRIPTION_SUFFIX_NAME]}
      <a href=${HELP_URL} target="_blank">${i18n[LINK_NAME]}</a>
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
