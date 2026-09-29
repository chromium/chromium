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
      <cr-button id="sitesButton" class="tonal-button"
          aria-haspopup="dialog" ?hidden="${this.sites_.length === 0}"
          @click="${this.onSitesButtonClick_}">
        <span id="siteFavicons" aria-hidden="true">
          ${this.getButtonFaviconUrls_().map(url => html`
            <span class="favicon"
                style="background-image: ${this.getFavicon_(url)}">
            </span>
          `)}
        </span>
        ${this.getSitesLabel_()}
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
        .topic="${this.topic}"
        @open-related-tabs="${this.onOpenRelatedTabs_}">
    </topic-summary-panel>
  </cr-page-selector>

  <topic-sites-dialog id="sitesDialog" .topic="${this.topic}"
      @open-related-tabs="${this.onOpenRelatedTabs_}">
  </topic-sites-dialog>
` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
