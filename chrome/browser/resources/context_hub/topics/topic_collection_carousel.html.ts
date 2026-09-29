// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicCollectionCarouselElement} from './topic_collection_carousel.js';

export function getHtml(this: TopicCollectionCarouselElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="header">
  <h2 id="title">${this.collection?.title || ''}</h2>
  <div id="scrollButtons"
      ?hidden="${!this.canScrollBack_ && !this.canScrollForward_}">
    <!-- TODO(crbug.com/558572977): Use internationalized strings once GRD -->
    <!-- strings are added. -->
    <cr-icon-button id="back" iron-icon="cr:chevron-left" aria-label="Back"
        @click="${this.onBackClick_}">
    </cr-icon-button>
    <cr-icon-button id="forward" iron-icon="cr:chevron-right"
        aria-label="Forward" @click="${this.onForwardClick_}">
    </cr-icon-button>
  </div>
</div>
<ul id="cards" aria-labelledby="title" @scroll="${this.onCardsScroll_}">
  ${(this.collection?.items || []).map((item, index) => html`
    <li>
      <button class="card" data-index="${index}" @click="${this.onCardClick_}">
        <span class="card-image" aria-hidden="true">
          <span class="card-favicon"
              style="background-image: ${this.getFavicon_(item.url)}">
          </span>
        </span>
        <span class="card-text">
          <span class="card-title" title="${item.title}">
            ${item.title}
          </span>
          <span class="card-site">${item.siteName}</span>
        </span>
      </button>
    </li>
  `)}
</ul>
<!--_html_template_end_-->`;
  // clang-format on
}
