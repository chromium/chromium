// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import './topic_collection_carousel.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_summary_panel.css.js';
import {getHtml} from './topic_summary_panel.html.js';
import {getOpenableUrls} from './topic_utils.js';
import type {TopicItem} from './topic_utils.js';

// The "Summary" tab of the topic details page: the topic's summary, then a
// carousel per collection. Its "Open related tabs" button fires an
// `open-related-tabs` event; the page owns opening the tabs.
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
    };
  }

  accessor topic: TopicItem|null = null;

  protected hasOpenableUrls_(): boolean {
    return !!this.topic && getOpenableUrls(this.topic).length > 0;
  }

  protected onOpenRelatedTabsClick_() {
    this.fire('open-related-tabs');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-summary-panel': TopicSummaryPanelElement;
  }
}

customElements.define(TopicSummaryPanelElement.is, TopicSummaryPanelElement);
