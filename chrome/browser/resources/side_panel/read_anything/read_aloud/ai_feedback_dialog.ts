// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './ai_feedback_dialog.css.js';
import {getHtml} from './ai_feedback_dialog.html.js';

export enum AiNegativeFeedbackReason {
  NOT_FACTUALLY_CORRECT = 'not-factually-correct',
  BAD_VOICE = 'bad-voice',
  NOT_ENGAGING = 'not-engaging',
  OFFENSIVE = 'offensive-content',
  TECHNICAL_ISSUE = 'technical-issue',
}

export interface AiFeedbackOption {
  id: AiNegativeFeedbackReason;
  label: string;
}

// Detail of the 'ai-feedback-submitted' event.
export interface AiFeedbackSubmittedDetail {
  reasons: AiNegativeFeedbackReason[];
  otherText: string;
}

export interface AiFeedbackDialogElement {
  $: {
    aiFeedbackDialog: CrDialogElement,
  };
}

const AiFeedbackDialogElementBase = I18nMixinLit(CrLitElement);

// The negative feedback dialog for AI Playback. The host creates a new
// instance each time it is opened, so it always starts with no reasons
// selected rather than restoring a previous selection.
export class AiFeedbackDialogElement extends AiFeedbackDialogElementBase {
  static get is() {
    return 'ai-feedback-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedReasons_: {type: Array},
    };
  }

  protected accessor selectedReasons_: AiNegativeFeedbackReason[] = [];

  protected readonly reasonOptions_: AiFeedbackOption[] = [
    {
      id: AiNegativeFeedbackReason.NOT_FACTUALLY_CORRECT,
      label: loadTimeData.getString(
          'aiPlaybackFeedbackReasonNotFactuallyCorrect'),
    },
    {
      id: AiNegativeFeedbackReason.BAD_VOICE,
      label: loadTimeData.getString('aiPlaybackFeedbackReasonBadVoice'),
    },
    {
      id: AiNegativeFeedbackReason.NOT_ENGAGING,
      label: loadTimeData.getString('aiPlaybackFeedbackReasonNotEngaging'),
    },
    {
      id: AiNegativeFeedbackReason.OFFENSIVE,
      label: loadTimeData.getString('aiPlaybackFeedbackReasonOffensive'),
    },
    {
      id: AiNegativeFeedbackReason.TECHNICAL_ISSUE,
      label: loadTimeData.getString('aiPlaybackFeedbackReasonTechnicalIssue'),
    },
  ];

  protected onReasonCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    const reason = (e.currentTarget as HTMLElement).dataset['reason'] as
        AiNegativeFeedbackReason;
    if (!reason) {
      return;
    }
    if (e.detail.value && !this.selectedReasons_.includes(reason)) {
      this.selectedReasons_ = [...this.selectedReasons_, reason];
    } else if (!e.detail.value) {
      this.selectedReasons_ =
          this.selectedReasons_.filter(item => item !== reason);
    }
  }

  protected canSubmit_(): boolean {
    return this.selectedReasons_.length > 0;
  }

  protected onCancelButtonClick_() {
    this.$.aiFeedbackDialog.cancel();
  }

  protected onSubmitClick_() {
    if (!this.canSubmit_()) {
      return;
    }
    const detail: AiFeedbackSubmittedDetail = {
      reasons: [...this.selectedReasons_],
      otherText: '',
    };
    this.fire('ai-feedback-submitted', detail);
    this.$.aiFeedbackDialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-feedback-dialog': AiFeedbackDialogElement;
  }
}

customElements.define(AiFeedbackDialogElement.is, AiFeedbackDialogElement);
