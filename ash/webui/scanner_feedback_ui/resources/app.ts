// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cros_components/orca_feedback/orca-feedback.js';
import '/strings.m.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {OrcaFeedback} from '//resources/cros_components/orca_feedback/orca-feedback.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {PageHandler} from './scanner_feedback_ui.mojom-webui.js';

const PAGE_HANDLER_REMOTE = PageHandler.getRemote();

const FEEDBACK_INFO_PROMISE = PAGE_HANDLER_REMOTE.getFeedbackInfo().then(
    ({feedbackInfo}) => feedbackInfo);


// `StringSource` is not exported from orca-feedback.js, so get a reference to
// it via `OrcaFeedback`.
type StringSource = NonNullable<OrcaFeedback['stringSource']>;

export class ScannerFeedbackAppElement extends I18nMixin
(PolymerElement) {
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
      extraInfoCallback: Object,
      screenshotUrl: String,
      revertToPreviousScreen: Object,
      submitFeedback: Object,
    };
  }

  // Used in template:
  private readonly stringSource: StringSource = {
    getAriaLabelForLink: (linkText: string): string => {
      return this.i18n('linkAriaLabel', linkText);
    },
    // We cannot use i18n template replacement here as it only works in JS files
    // in HTML string templates. Additionally, double quotes are HTML escaped
    // with template replacement, which is not suitable for JS use as
    // localisation strings are already escaped by Polymer/Lit.
    MSG_FEEDBACK_TITLE: this.i18n('title'),
    MSG_FEEDBACK_SUBTITLE: this.i18n('subtitle'),
    MSG_FEEDBACK_QUESTION: this.i18n('question'),
    MSG_FEEDBACK_QUESTION_PLACEHOLDER: this.i18n('questionPlaceholder'),
    MSG_OFFENSIVE_OR_UNSAFE: this.i18n('offensiveOrUnsafe'),
    MSG_FACTUALLY_INCORRECT: this.i18n('factuallyIncorrect'),
    MSG_LEGAL_ISSUE: this.i18n('legalIssue'),
    getFeedbackDisclaimer:
        (linkPolicyBegin: string, linkPolicyEnd: string,
         linkTermsOfServiceBegin: string, linkTermsOfServiceEnd: string):
            string => {
              return this
                  .i18nAdvanced('feedbackDisclaimer', {
                    substitutions: [
                      linkPolicyBegin,
                      linkPolicyEnd,
                      linkTermsOfServiceBegin,
                      linkTermsOfServiceEnd,
                    ],
                    attrs: ['id', 'class', 'role', 'aria-label'],
                  })
                  // <mako-orca-feedback> uses `maybeSafeHTML` which expects
                  // plain strings. As of writing, `maybeSafeHTML` does not
                  // perform any sanitization (b/338151548). However, this is
                  // safe as the `i18nAdvanced` call above *does* perform
                  // sanitization, and will be sandboxed under this
                  // chrome-untrusted:// origin in the worst case.
                  .toString();
            },
    MSG_PRIVACY_POLICY: this.i18n('privacyPolicy'),
    MSG_TERMS_OF_SERVICE: this.i18n('termsOfService'),
    MSG_CANCEL: this.i18n('cancel'),
    MSG_SEND: this.i18n('send'),
  };
  private readonly openUrl = (url: string) => {
    window.open(url, '_blank');
    this.revertToPreviousScreen();
  };
  private readonly extraInfoCallback = () =>
      FEEDBACK_INFO_PROMISE.then(feedbackInfo => feedbackInfo.actionDetails);
  // This value has two null states: undefined, meaning that the feedback info
  // Promise has not resolved yet, and empty string, meaning that the feedback
  // info Promise resolved with a null screenshot URL.
  private screenshotUrl?: string;
  private readonly revertToPreviousScreen = () =>
      PAGE_HANDLER_REMOTE.closeDialog();
  private readonly submitFeedback = (userDescription: string) =>
      PAGE_HANDLER_REMOTE.sendFeedback(userDescription);

  constructor() {
    super();
    FEEDBACK_INFO_PROMISE.then(feedbackInfo => {
      this.screenshotUrl = feedbackInfo.screenshotUrl?.url ?? '';
    });
  }

  // Used in computed bindings:
  private isUndefined(value: unknown): value is undefined {
    return value === undefined;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ScannerFeedbackAppElement.is]: ScannerFeedbackAppElement;
  }
}

customElements.define(ScannerFeedbackAppElement.is, ScannerFeedbackAppElement);

document.addEventListener('DOMContentLoaded', () => {
  ColorChangeUpdater.forDocument().start();
});
