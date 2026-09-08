// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopicsViewElement} from './topics_view.js';

export function getHtml(this: TopicsViewElement) {
  return html`
    <main id="topics-view">
      <section>
        <div class="header-container">
          <h1>Topics</h1>
        </div>

        <p>No topics yet.</p>
      </section>
    </main>
  `;
}
