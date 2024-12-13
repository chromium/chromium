// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cros_components/orca_feedback/orca-feedback.js';

import type {OrcaFeedback} from '//resources/cros_components/orca_feedback/orca-feedback.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

// `StringSource` is not exported from orca-feedback.js, so get a reference to
// it via `OrcaFeedback`.
type StringSource = NonNullable<OrcaFeedback['stringSource']>;

const STRING_SOURCE: StringSource = {
  getAriaLabelForLink(linkText: string): string {
    return `ARIA LABEL FOR ${linkText}`;
  },
  MSG_FEEDBACK_TITLE: 'FEEDBACK TITLE',
  MSG_FEEDBACK_SUBTITLE: 'FEEDBACK SUBTITLE',
  MSG_FEEDBACK_QUESTION: 'FEEDBACK QUESTION',
  MSG_FEEDBACK_QUESTION_PLACEHOLDER: 'FEEDBACK QUESTION PLACEHOLDER',
  MSG_OFFENSIVE_OR_UNSAFE: 'OFFENSIVE OR UNSAFE',
  MSG_FACTUALLY_INCORRECT: 'FACTUALLY INCORRECT',
  MSG_LEGAL_ISSUE: 'LEGAL ISSUE',
  getFeedbackDisclaimer(
      linkPolicyBegin: string, linkPolicyEnd: string,
      linkTermsOfServiceBegin: string, linkTermsOfServiceEnd: string): string {
    return `FEEDBACK DISCLAIMER ${linkPolicyBegin}POLICY${linkPolicyEnd} ${
        linkTermsOfServiceBegin}TERMS OF SERVICE${linkTermsOfServiceEnd}`;
  },
  MSG_PRIVACY_POLICY: 'PRIVACY POLICY',
  MSG_TERMS_OF_SERVICE: 'TERMS OF SERVICE',
  MSG_CANCEL: 'CANCEL',
  MSG_SEND: 'SEND'
};

export class ScannerFeedbackAppElement extends PolymerElement {
  static get is() {
    return 'scanner-feedback-app' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Private properties:
      stringSource: Object,
      openUrl: Object,
    };
  }

  private readonly stringSource = STRING_SOURCE;
  private readonly openUrl = (url: string) => {
    window.open(url, '_blank');
    // TODO: b/382562555 - Close the dialog after opening the URL.
  };
}

declare global {
  interface HTMLElementTagNameMap {
    [ScannerFeedbackAppElement.is]: ScannerFeedbackAppElement;
  }
}

customElements.define(ScannerFeedbackAppElement.is, ScannerFeedbackAppElement);
