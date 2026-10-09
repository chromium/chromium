// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '//resources/cr_elements/icons.html.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {TopicDefectCategory, TopicRating} from '../context_hub.mojom-webui.js';

import {getCss} from './topic_feedback_controls.css.js';
import {getHtml} from './topic_feedback_controls.html.js';
import {createEmptyTopicFeedback, createTopicSnapshot, formatTopicFeedbackComment, hasTopicFeedbackCommentText, isTopicFeedbackValid, normalizeTopicFeedbackComment, parseTopicFeedbackComment, TOPIC_DEFECT_CATEGORIES, TOPIC_SUGGESTION_FIELDS} from './topic_utils.js';
import type {TopicFeedback, TopicItem, TopicSuggestionField} from './topic_utils.js';

// Fired with the feedback to store whenever the rater's edits are valid.
export type TopicFeedbackChangeEvent = CustomEvent<{feedback: TopicFeedback}>;

// Fishfood-only controls for rating a topic: thumbs up/down and, on a thumbs
// down, defect chips and a comment, plus a field asking for a better title,
// emoji or overview when that's the defect. Reports valid edits as
// `topic-feedback-change`; storing them is up to the embedder. Only render it
// when `isTopicsFishfoodFeedbackEnabled()` is true.
// TODO(crbug.com/558572977): Use internationalized strings once GRD
// strings are added.
export class TopicFeedbackControlsElement extends CrLitElement {
  static get is() {
    return 'topic-feedback-controls';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      topic: {type: Object},
      // The stored feedback on `topic`, if any.
      feedback: {type: Object},
      draft_: {type: Object, state: true},
    };
  }

  accessor topic: TopicItem|null = null;
  accessor feedback: TopicFeedback|null = null;
  // The feedback being edited. Only reported via `topic-feedback-change`
  // while it's valid, so it can be ahead of `feedback`. Always replaced rather
  // than mutated, so it can share arrays with `feedback`.
  protected accessor draft_: TopicFeedback|null = null;

  protected readonly defectCategories_ = TOPIC_DEFECT_CATEGORIES;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('feedback')) {
      this.draft_ = this.feedback;
    }
  }

  protected getThumbsUpAriaLabel_(): string {
    return this.getAriaLabel_('Good topic:', 'Good topic');
  }

  protected getThumbsDownAriaLabel_(): string {
    return this.getAriaLabel_('Bad topic:', 'Bad topic');
  }

  // Names the topic, since these controls can be repeated on every card.
  private getAriaLabel_(prefix: string, fallback: string): string {
    return this.topic?.title.trim() ? `${prefix} ${this.topic.title}` :
                                      fallback;
  }

  protected isLiked_(): boolean {
    return this.draft_?.rating === TopicRating.kLiked;
  }

  protected isDisliked_(): boolean {
    return this.draft_?.rating === TopicRating.kDisliked;
  }

  protected isDefectSelected_(category: TopicDefectCategory): boolean {
    return !!this.draft_?.defects.includes(category);
  }

  protected getComment_(): string {
    return parseTopicFeedbackComment(this.draft_?.comment || '').text;
  }

  // The suggestion fields for the selected defects.
  protected getSuggestionFields_(): TopicSuggestionField[] {
    return TOPIC_SUGGESTION_FIELDS.filter(
        field => this.isDefectSelected_(field.defect));
  }

  protected getSuggestion_(defect: TopicDefectCategory): string {
    return parseTopicFeedbackComment(this.draft_?.comment || '')
               .suggestions.get(defect) ||
        '';
  }

  protected isCommentRequired_(): boolean {
    return this.isDefectSelected_(TopicDefectCategory.kOther);
  }

  protected isCommentMissing_(): boolean {
    return !!this.draft_ && this.isCommentRequired_() &&
        !hasTopicFeedbackCommentText(this.draft_);
  }

  protected getCommentError_(): string {
    return this.isCommentMissing_() ?
        'Describe the issue in a comment when "Other" is selected' :
        '';
  }

  protected isDefectMissing_(): boolean {
    return this.isDisliked_() && this.draft_?.defects.length === 0;
  }

  protected onThumbsUpClick_() {
    this.setRating_(
        this.isLiked_() ? TopicRating.kUnrated : TopicRating.kLiked);
  }

  protected onThumbsDownClick_() {
    this.setRating_(
        this.isDisliked_() ? TopicRating.kUnrated : TopicRating.kDisliked);
  }

  protected onDefectClick_(e: Event) {
    const category =
        Number((e.currentTarget as HTMLElement).dataset['category']) as
        TopicDefectCategory;
    const draft = this.getOrCreateDraft_();
    if (!draft) {
      return;
    }
    if (!draft.defects.includes(category)) {
      this.updateDraft_({...draft, defects: [...draft.defects, category]});
      return;
    }
    // A suggestion only applies while its defect is selected.
    const {text, suggestions} = parseTopicFeedbackComment(draft.comment);
    suggestions.delete(category);
    this.updateDraft_({
      ...draft,
      defects: draft.defects.filter(defect => defect !== category),
      comment: formatTopicFeedbackComment(text, suggestions),
    });
  }

  protected onCommentValueChanged_(e: CustomEvent<{value: string}>) {
    const draft = this.getOrCreateDraft_();
    if (!draft) {
      return;
    }
    const {suggestions} = parseTopicFeedbackComment(draft.comment);
    this.setDraftComment_(
        draft, formatTopicFeedbackComment(e.detail.value, suggestions));
  }

  protected onSuggestionValueChanged_(e: CustomEvent<{value: string}>) {
    const defect =
        Number((e.currentTarget as HTMLElement).dataset['defect']) as
        TopicDefectCategory;
    const draft = this.getOrCreateDraft_();
    if (!draft) {
      return;
    }
    const {text, suggestions} = parseTopicFeedbackComment(draft.comment);
    suggestions.set(defect, e.detail.value);
    this.setDraftComment_(draft, formatTopicFeedbackComment(text, suggestions));
  }

  private setDraftComment_(draft: TopicFeedback, comment: string) {
    if (draft.comment === comment) {
      return;
    }
    // Only saved on `change`, so that every keystroke isn't persisted.
    this.draft_ = {...draft, comment};
  }

  protected onCommentChange_(e: Event) {
    // Keep the textarea's and inputs' `change` from reaching the page; the
    // draft is reported as `topic-feedback-change` instead.
    e.stopPropagation();
    // `value-changed` already copied the comment to the draft.
    if (this.draft_) {
      this.updateDraft_(this.draft_);
    }
  }

  private setRating_(rating: TopicRating) {
    const draft = this.getOrCreateDraft_();
    if (!draft) {
      return;
    }
    // Defects and the comment only apply to a thumbs down, so drop them
    // otherwise.
    if (rating !== TopicRating.kDisliked) {
      this.updateDraft_({...draft, rating, defects: [], comment: ''});
      return;
    }
    this.updateDraft_({...draft, rating});
  }

  private getOrCreateDraft_(): TopicFeedback|null {
    if (!this.topic) {
      return null;
    }
    return this.draft_ || createEmptyTopicFeedback(this.topic);
  }

  // Updates the draft, and reports it if it can be saved.
  private updateDraft_(draft: TopicFeedback) {
    this.draft_ = draft;
    if (!this.topic || !isTopicFeedbackValid(draft)) {
      return;
    }
    // Snapshot the topic as it is now, since that's what was rated.
    const feedback: TopicFeedback = {
      ...draft,
      comment: normalizeTopicFeedbackComment(draft.comment),
      snapshot: createTopicSnapshot(this.topic),
    };
    this.fire('topic-feedback-change', {feedback});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-feedback-controls': TopicFeedbackControlsElement;
  }
}

customElements.define(
    TopicFeedbackControlsElement.is, TopicFeedbackControlsElement);
