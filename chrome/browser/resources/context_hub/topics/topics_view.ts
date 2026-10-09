// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import './topic_card.js';

import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';

import type {TopicFeedbackChangeEvent} from './topic_feedback_controls.js';
import {isTopicFeedbackEmpty, isTopicsEnabled, isTopicsFishfoodFeedbackEnabled, toTopicItem} from './topic_utils.js';
import type {TopicFeedback, TopicItem} from './topic_utils.js';
import {getCss} from './topics_view.css.js';
import {getHtml} from './topics_view.html.js';

export type {TopicItem} from './topic_utils.js';

export type TopicsLoadState = 'loading'|'loaded'|'error';

// TODO(crbug.com/558572977): Use internationalized strings once GRD
// strings are added.
export class TopicsViewElement extends CrLitElement {
  static get is() {
    return 'topics-view';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      topics: {type: Array},
      loadState_: {type: String},
      feedbacks_: {type: Object},
      feedbackEnabled_: {type: Boolean},
    };
  }

  accessor topics: TopicItem[] = [];
  // Nothing is rendered while loading, so that "No topics yet." doesn't flash
  // up before the topics arrive.
  protected accessor loadState_: TopicsLoadState = 'loading';
  // Stored fishfood feedback, by topic id. Only loaded when fishfood feedback
  // is enabled.
  protected accessor feedbacks_: Map<string, TopicFeedback> = new Map();
  protected accessor feedbackEnabled_: boolean =
      isTopicsFishfoodFeedbackEnabled();

  override connectedCallback() {
    super.connectedCallback();
    this.fetchTopics_();
    document.addEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
  }

  // Topics can also be rated on their details page, which opens in another
  // tab, so reload the ratings whenever this tab is shown again.
  private onVisibilityChange_ = () => {
    if (document.visibilityState === 'visible') {
      this.refreshFeedbacks_();
    }
  };

  private async fetchTopics_() {
    // `GetTopics()` is gated by the kTopics runtime feature in the browser
    // process, so don't call it when the feature is off.
    if (!isTopicsEnabled()) {
      this.loadState_ = 'loaded';
      return;
    }
    this.loadState_ = 'loading';
    try {
      const [{topics}, feedbacks] = await Promise.all([
        browserProxyFactory.getInstance().handler.getTopics(),
        this.fetchFeedbacks_(),
      ]);
      this.feedbacks_ =
          new Map(feedbacks.map(feedback => [feedback.id, feedback]));
      this.topics = topics.map(toTopicItem);
      this.loadState_ = 'loaded';
    } catch (e) {
      console.error('Failed to fetch topics:', e);
      this.loadState_ = 'error';
    }
  }

  // The topics still load if the feedback can't, just without ratings.
  private async fetchFeedbacks_(): Promise<TopicFeedback[]> {
    // The feedback methods are gated in the browser process too.
    if (!this.feedbackEnabled_) {
      return [];
    }
    try {
      const {feedbacks} =
          await browserProxyFactory.getInstance().handler.getTopicFeedbacks();
      return feedbacks;
    } catch (e) {
      console.error('Failed to fetch topic feedback:', e);
      return [];
    }
  }

  // Unlike the initial load, keeps the current ratings if reloading fails.
  private async refreshFeedbacks_() {
    if (!this.feedbackEnabled_ || this.loadState_ !== 'loaded') {
      return;
    }
    try {
      const {feedbacks} =
          await browserProxyFactory.getInstance().handler.getTopicFeedbacks();
      this.feedbacks_ =
          new Map(feedbacks.map(feedback => [feedback.id, feedback]));
    } catch (e) {
      console.error('Failed to reload topic feedback:', e);
    }
  }

  protected isEmpty_(): boolean {
    return this.loadState_ === 'loaded' && this.topics.length === 0;
  }

  protected hasTopics_(): boolean {
    return this.loadState_ === 'loaded' && this.topics.length > 0;
  }

  protected getFeedback_(topic: TopicItem): TopicFeedback|null {
    return this.feedbacks_.get(topic.id) || null;
  }

  protected onTopicFeedbackChange_(e: TopicFeedbackChangeEvent) {
    const feedback = e.detail.feedback;
    if (!this.feedbackEnabled_ || !feedback.id) {
      return;
    }
    const handler = browserProxyFactory.getInstance().handler;
    const feedbacks = new Map(this.feedbacks_);
    if (isTopicFeedbackEmpty(feedback)) {
      handler.deleteTopicFeedback(feedback.id);
      feedbacks.delete(feedback.id);
    } else {
      handler.setTopicFeedback(feedback);
      feedbacks.set(feedback.id, feedback);
    }
    this.feedbacks_ = feedbacks;
  }

  protected onSendFeedbackClick_() {
    this.fire('send-feedback-click');
  }

  protected onJumpBackIn_(e: CustomEvent<{topic: TopicItem}>) {
    const topic = e.detail.topic;
    if (!topic.id) {
      return;
    }

    // Only the id is passed; the details page fetches the topic itself, so
    // that it always shows current data and nothing in the URL is trusted.
    const params = new URLSearchParams();
    params.set('id', topic.id);
    // Signals the topic details page to open the Glic side panel once it
    // loads. Only this entry point sets it, so a topic page reached without it
    // (e.g. a hand-typed URL) never forces the side panel open. It does persist
    // in the URL, so reload and session restore reopen the panel; see
    // TopicDetailsElement.maybeOpenGlicPanel_ for why that is intentional.
    // Whether Glic is actually available is decided browser-side in
    // PageHandler::OpenGlicPanel, which no-ops when it is not.
    params.set('open_glic', '1');
    const topicUrl = `chrome://context-hub/topic_details?${params.toString()}`;

    const handler = browserProxyFactory.getInstance().handler;
    if (handler) {
      handler.openTopic({topicUrl});
    } else {
      OpenWindowProxyImpl.getInstance().openUrl(topicUrl);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topics-view': TopicsViewElement;
  }
}

customElements.define(TopicsViewElement.is, TopicsViewElement);
