// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicDetailsElement} from './topic_details.js';

export function getHtml(this: TopicDetailsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="wrapper" aria-busy="${this.loadState_ === 'loading'}"
    @scroll="${this.onScroll_}">
${this.loadState_ === 'not-found' ? html`
  <!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
  <!-- strings are added. -->
  <p id="notFound" class="status-message" role="status">
    This topic is no longer available.
  </p>
` : ''}
${this.loadState_ === 'loaded' && this.topic ? html`
  <!-- Layer 1: behind the header and content, scrolls away. -->
  <topic-hero .topic="${this.topic}"></topic-hero>

  <!-- Layer 3: sticks to the top. -->
  <div id="header" class="${this.isScrolled_ ? 'scrolled' : ''}">
    <div class="title-row">
      <h1 id="title">${this.topic.title}</h1>
      <!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
      <!-- strings are added. -->
      <cr-button id="openRelatedTabs" class="tonal-button"
          ?hidden="${!this.hasOpenableUrls_()}"
          @click="${this.onOpenRelatedTabsClick_}">
        Open related tabs
      </cr-button>
    </div>
    <cr-tabs id="tabs" .tabNames="${this.tabNames_}"
        .selected="${this.selectedTab_}"
        @selected-changed="${this.onTabsSelectedChanged_}">
    </cr-tabs>
  </div>

  <!-- Layer 2: scrolls above the hero and under the header. One panel per
       entry in TOPIC_DETAILS_TABS, in the same order. -->
  <cr-page-selector id="panels" .selected="${this.selectedTab_}">
    <topic-summary-panel role="tabpanel" aria-label="${this.tabNames_[0]}"
        .topic="${this.topic}">
    </topic-summary-panel>
  </cr-page-selector>
` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
