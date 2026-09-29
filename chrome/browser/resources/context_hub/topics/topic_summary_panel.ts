// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_summary_panel.css.js';
import {getHtml} from './topic_summary_panel.html.js';
import type {TopicItem} from './topic_utils.js';

// The "Summary" tab of the topic details page.
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
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-summary-panel': TopicSummaryPanelElement;
  }
}

customElements.define(TopicSummaryPanelElement.is, TopicSummaryPanelElement);
