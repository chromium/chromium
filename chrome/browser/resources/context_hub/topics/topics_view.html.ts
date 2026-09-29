// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicsViewElement} from './topics_view.js';

export function getHtml(this: TopicsViewElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <main id="topics-view" aria-busy="${this.loadState_ === 'loading'}">
      <section class="container">
        <div class="header-container">
          <h1>Topics</h1>
        </div>

        ${this.loadState_ === 'error' ? html`
          <p id="errorState" class="empty-state" role="status">
            Couldn't load topics.
          </p>
        ` : ''}
        ${this.isEmpty_() ? html`
          <p id="emptyState" class="empty-state" role="status">
            No topics yet.
          </p>
        ` : ''}
        ${this.hasTopics_() ? html`
          <div id="cardsList" class="cards-list" role="list"
              aria-label="Topics">
            ${repeat(this.topics, topic => topic.id, topic => html`
              <topic-card role="listitem" .topic="${topic}"
                  @jump-back-in="${this.onJumpBackIn_}">
              </topic-card>
            `)}
          </div>
        ` : ''}
      </section>
    </main>
  <!--_html_template_end_-->`;
  // clang-format on
}
