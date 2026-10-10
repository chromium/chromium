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
  ${this.feedbackEnabled_ ? html`
    <!-- Temporary Fishfood Feedback -->
    <div id="collectionRating">
      <cr-icon-button class="collection-like" data-liked="true"
          iron-icon="${this.isRated_(true) ?
              'cr:thumb-up-filled' : 'cr:thumb-up'}"
          title="Useful collection"
          aria-label="${this.getRatingAriaLabel_('Useful collection')}"
          aria-pressed="${this.isRated_(true)}"
          @click="${this.onRatingClick_}">
      </cr-icon-button>
      <cr-icon-button class="collection-dislike" data-liked="false"
          iron-icon="${this.isRated_(false) ?
              'cr:thumb-down-filled' : 'cr:thumb-down'}"
          title="Not a useful collection"
          aria-label="${this.getRatingAriaLabel_('Not a useful collection')}"
          aria-pressed="${this.isRated_(false)}"
          @click="${this.onRatingClick_}">
      </cr-icon-button>
    </div>
  ` : ''}
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
    <li ?rejected="${this.feedbackEnabled_ && this.isItemRejected_(item)}">
      <button class="card" data-index="${index}" @click="${this.onCardClick_}">
        <span class="card-image" aria-hidden="true">
          ${this.getImageUrl_(item.url) ? html`
            <img class="card-page-image" is="cr-auto-img"
                auto-src="${this.getImageUrl_(item.url)}"
                data-url="${item.url}" alt=""
                @error="${this.onCardImageError_}">
          ` : html`
            <span class="card-favicon"
                style="background-image: ${this.getFavicon_(item.url)}">
            </span>
          `}
        </span>
        <span class="card-text">
          <span class="card-title" title="${item.title}">
            ${item.title}
          </span>
          <span class="card-site">${item.siteName}</span>
        </span>
      </button>
      ${this.feedbackEnabled_ ? html`
        <!-- Temporary Fishfood Feedback. A sibling of the card, since a
             button can't hold another. -->
        ${this.isItemRejected_(item) ? html`
          <span class="rejected-label" aria-hidden="true">Doesn't belong</span>
        ` : ''}
        <cr-icon-button class="reject" data-index="${index}"
            iron-icon="${this.isItemRejected_(item) ?
                'cr:cancel-filled' : 'cr:close'}"
            title="${this.isItemRejected_(item) ? 'Undo' : 'Doesn\'t belong'}"
            aria-label="${this.getRejectAriaLabel_(item)}"
            aria-pressed="${this.isItemRejected_(item)}"
            @click="${this.onRejectClick_}">
        </cr-icon-button>
      ` : ''}
    </li>
  `)}
</ul>
<!--_html_template_end_-->`;
  // clang-format on
}
