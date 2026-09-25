// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './topic_card.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';

import {toTopicItem} from './topic_card.js';
import type {TopicItem} from './topic_card.js';
import {getCss} from './topics_view.css.js';
import {getHtml} from './topics_view.html.js';

export type {TopicItem} from './topic_card.js';

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
