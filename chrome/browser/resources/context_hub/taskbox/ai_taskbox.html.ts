// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AiTaskboxElement} from './ai_taskbox.js';

export function getHtml(this: AiTaskboxElement) {
  return html`
    <main id="dashboard-view">
        <section>
            <h1>AI Taskbox</h1>
        </section>
    </main>
  `;
}
