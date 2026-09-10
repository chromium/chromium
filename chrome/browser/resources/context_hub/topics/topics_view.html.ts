// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicsViewElement} from './topics_view.js';

export function getHtml(this: TopicsViewElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <main id="topics-view">
      <section class="container">
        <div class="header-container">
          <h1>Topics</h1>
        </div>

        ${this.topics.length === 0 ? html`
          <p class="empty-state">No topics yet.</p>
        ` : html`
          <div class="cards-list" role="list" aria-label="Topics">
            ${repeat(
                this.topics,
                topic => topic.id,
                (topic, index) => html`
                  <topic-card
                      role="listitem"
                      .topic="${topic}"
                      .index="${index}"
                      @jump-back-in="${this.onJumpBackIn_}">
                  </topic-card>
                `)}
          </div>
        `}
      </section>
    </main>
  <!--_html_template_end_-->`;
  // clang-format on
}
