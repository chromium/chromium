// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './topic_card.js';

import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

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
    // TODO(crbug.com/558572977): Query topics from HistoryService KeyedService.
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
