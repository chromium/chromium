// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import './topic_collection_carousel.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {CollectionFeedbackChangeEvent} from './topic_collection_carousel.js';
import {getCss} from './topic_summary_panel.css.js';
import {getHtml} from './topic_summary_panel.html.js';
import {getOpenableUrls, getTopicQueries, isCollectionFeedbackEmpty, isTopicsFishfoodFeedbackEnabled} from './topic_utils.js';
import type {CollectionFeedback, TopicFeedback, TopicItem, TopicQuery, TopicQueryFeedback} from './topic_utils.js';

// Fired with the rated queries whenever the rater rates one.
export type QueryFeedbacksChangeEvent =
    CustomEvent<{queryFeedbacks: TopicQueryFeedback[]}>;

// The "Summary" tab of the topic details page: the topic's summary, then a
// carousel per collection. Its "Open related tabs" button fires an
// `open-related-tabs` event; the page owns opening the tabs.
//
// When fishfood feedback is enabled, it also lists the topic's continuation
// queries for rating, and fires `query-feedbacks-change` when one is rated;
// the page owns storing the feedback.
// TODO(crbug.com/558572977): Use internationalized strings once GRD strings
// are added.
export class TopicSummaryPanelElement extends CrLitElement {
  static get is() {
    return 'topic-summary-panel';
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
      // The stored fishfood feedback on `topic`, if any.
      feedback: {type: Object},
      feedbackEnabled_: {type: Boolean},
      queries_: {type: Array},
      collectionFeedbacks_: {type: Object},
    };
  }

  accessor topic: TopicItem|null = null;
  accessor feedback: TopicFeedback|null = null;
  protected accessor feedbackEnabled_: boolean =
      isTopicsFishfoodFeedbackEnabled();
  // The queries to rate. Derived from `topic`.
  protected accessor queries_: TopicQuery[] = [];
  // Feedback on the topic's collections, keyed by index in
  // `topic.collections`.
  // TODO(b/568422896): Store in TopicFeedback, via the page, once the mojom
  // has fields for collections. Until then it's lost on reload.
  protected accessor collectionFeedbacks_: Map<number, CollectionFeedback> =
      new Map();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('topic')) {
      this.queries_ = this.topic ? getTopicQueries(this.topic) : [];
      this.collectionFeedbacks_ = new Map();
    }
  }

  protected getCollectionFeedback_(index: number): CollectionFeedback|null {
    return this.collectionFeedbacks_.get(index) || null;
  }

  protected onCollectionFeedbackChange_(e: CollectionFeedbackChangeEvent) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const collectionFeedbacks = new Map(this.collectionFeedbacks_);
    if (isCollectionFeedbackEmpty(e.detail.feedback)) {
      collectionFeedbacks.delete(index);
    } else {
      collectionFeedbacks.set(index, e.detail.feedback);
    }
    this.collectionFeedbacks_ = collectionFeedbacks;
  }

  protected hasOpenableUrls_(): boolean {
    return !!this.topic && getOpenableUrls(this.topic).length > 0;
  }

  protected onOpenRelatedTabsClick_() {
    this.fire('open-related-tabs');
  }

  // Whether the query at `index` in `topic.continuationQueries` is rated
  // `liked`.
  protected isQueryRated_(index: number, liked: boolean): boolean {
    return !!this.feedback?.queryFeedbacks.some(
        rated => rated.index === index && rated.liked === liked);
  }

  protected getQueryAriaLabel_(prefix: string, query: TopicQuery): string {
    return `${prefix}: ${query.text}`;
  }

  protected onQueryRatingClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const liked = target.dataset['liked'] === 'true';
    const query = this.queries_.find(query => query.index === index);
    if (!query) {
      return;
    }
    // Clicking the selected rating again clears it. Only rated queries are
    // stored.
    const others = (this.feedback?.queryFeedbacks ||
                    []).filter(rated => rated.index !== index);
    const queryFeedbacks = this.isQueryRated_(index, liked) ? others : [
      ...others,
      {index, queryText: query.text, liked},
    ].sort((a, b) => a.index - b.index);
    this.fire('query-feedbacks-change', {queryFeedbacks});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-summary-panel': TopicSummaryPanelElement;
  }
}

customElements.define(TopicSummaryPanelElement.is, TopicSummaryPanelElement);
