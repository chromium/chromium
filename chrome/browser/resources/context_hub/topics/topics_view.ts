// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './topic_card.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {Topic} from '../context_hub.mojom-webui.js';

import type {TopicItem} from './topic_card.js';
import {getCss} from './topics_view.css.js';
import {getHtml} from './topics_view.html.js';

export type {TopicItem} from './topic_card.js';

// Adapts a Topic from the browser process to what the cards render. The
// backend already resolved every visit to a URL and a title, so this is a
// pure field mapping.
function toTopicItem(topic: Topic): TopicItem {
  return {
    id: topic.id,
    title: topic.title,
    // The short summary is what the card has room for; fall back to the long
    // one when the server only sent that.
    description: topic.shortOverview || topic.overview || '',
    longDescription: topic.overview || undefined,
    // `topic-card` renders any icon string without a colon as literal text,
    // which is what an emoji needs.
    icon: topic.emoji || undefined,
    relatedUrls: topic.visits.map(visit => visit.url),
  };
}

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
    };
  }

  accessor topics: TopicItem[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.fetchTopics_();
  }

  private async fetchTopics_() {
    // `GetTopics()` is gated by the kTopics runtime feature in the browser
    // process, so don't call it when the feature is off.
    if (!loadTimeData.valueExists('kTopics') ||
        !loadTimeData.getBoolean('kTopics')) {
      return;
    }
    const {topics} =
        await browserProxyFactory.getInstance().handler.getTopics();
    this.topics = topics.map(toTopicItem);
  }

  protected onJumpBackIn_(e: CustomEvent<{topic: TopicItem}>) {
    const topic = e.detail.topic;
    try {
      // TODO(crbug.com/558572977): Fetch the topic directly by id over Mojo
      // from C++ once connected to HistoryService KeyedService.
      sessionStorage.setItem('active_topic', JSON.stringify(topic));
      if (topic.id) {
        sessionStorage.setItem(
            `context_hub_topic_${topic.id}`, JSON.stringify(topic));
      }
    } catch {
      // Ignore storage errors.
    }

    const params = new URLSearchParams();
    if (topic.id) {
      params.set('id', topic.id);
    }
    if (topic.badgeShape) {
      params.set('shape', topic.badgeShape);
    }
    if (topic.icon) {
      params.set('icon', topic.icon);
    }
    if (topic.title) {
      params.set('title', topic.title);
    }
    if (topic.backgroundColor) {
      params.set('bg', topic.backgroundColor);
    }
    const queryString = params.toString();
    const topicUrl = queryString ?
        `chrome://context-hub/topic_details.html?${queryString}` :
        'chrome://context-hub/topic_details.html';
    OpenWindowProxyImpl.getInstance().openUrl(topicUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topics-view': TopicsViewElement;
  }
}

customElements.define(TopicsViewElement.is, TopicsViewElement);
