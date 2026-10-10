// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {formatTopicTime, getDisplayDomain, isVisitRejected, setVisitsRejected} from './topic_utils.js';
import type {TopicFeedback, TopicItem, TopicVisit} from './topic_utils.js';
import {getCss} from './topic_visits_panel.css.js';
import {getHtml} from './topic_visits_panel.html.js';

// Fired with the visits the rater flagged as not belonging to the topic,
// whenever they change.
export type RejectedVisitsChangeEvent =
    CustomEvent<{rejectedVisits: TopicFeedback['rejectedVisits']}>;

// A row of the panel: a visit plus what's displayed for it, computed once per
// topic.
export interface VisitRow extends TopicVisit {
  domain: string;
  time: string;
}

// The fishfood-only "Topic Visits (Fishfood)" tab of the topic details page.
// Lists every visit of the topic, most recent first, each with a "Doesn't
// belong" toggle. Fires `rejected-visits-change`; the page owns storing the
// feedback. Only render it when `isTopicsFishfoodFeedbackEnabled()` is true.
// TODO(crbug.com/558572977): Use internationalized strings once GRD strings
// are added.
export class TopicVisitsPanelElement extends CrLitElement {
  static get is() {
    return 'topic-visits-panel';
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
      visits_: {type: Array},
    };
  }

  accessor topic: TopicItem|null = null;
  accessor feedback: TopicFeedback|null = null;
  protected accessor visits_: VisitRow[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('topic')) {
      this.visits_ = (this.topic?.visits || [])
                         .map(visit => ({
                                ...visit,
                                domain: getDisplayDomain(visit.url),
                                time: formatTopicTime(visit.visitTime),
                              }))
                         .sort((a, b) => {
                           const diff = b.visitTime.internalValue -
                               a.visitTime.internalValue;
                           return diff === 0n ? 0 : (diff > 0n ? 1 : -1);
                         });
    }
  }

  protected getFavicon_(url: string): string {
    return getFaviconForPageURL(url, /*isSyncedUrlForHistoryUi=*/ false);
  }

  protected isRejected_(visit: TopicVisit): boolean {
    return isVisitRejected(
        this.feedback?.rejectedVisits || [], visit.visitTime);
  }

  protected getRejectAriaLabel_(visit: VisitRow): string {
    return `Doesn't belong: ${visit.title || visit.domain}`;
  }

  protected onRejectClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const visit = this.visits_[index];
    if (!visit) {
      return;
    }
    this.fire('rejected-visits-change', {
      rejectedVisits: setVisitsRejected(
          this.feedback?.rejectedVisits || [], [visit.visitTime],
          !this.isRejected_(visit)),
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-visits-panel': TopicVisitsPanelElement;
  }
}

customElements.define(TopicVisitsPanelElement.is, TopicVisitsPanelElement);
