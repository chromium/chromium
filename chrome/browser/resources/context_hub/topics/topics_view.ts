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

// TODO(crbug.com/558572977): Use internationalized strings once GRD strings are added.
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
    const destinationUrl = e.detail.topic.destinationUrl;
    if (destinationUrl) {
      OpenWindowProxyImpl.getInstance().openUrl(destinationUrl);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topics-view': TopicsViewElement;
  }
}

customElements.define(TopicsViewElement.is, TopicsViewElement);
