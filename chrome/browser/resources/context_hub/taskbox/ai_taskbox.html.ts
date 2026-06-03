// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {AiTaskboxElement} from './ai_taskbox.js';

export function getHtml(this: AiTaskboxElement) {
  return html`
    <main id="dashboard-view">
        <section>
            <!-- TODO(crbug.com/519576944): Replace with the dynamic greeting title. -->
            <h1>AI Taskbox</h1>
        </section>

        <!-- Suggested To-Dos Section-->
        <section>
            <div class="section-header">
                <h2>Suggested to-dos</h2>
            </div>

            <!-- TODO(crbug.com/519576944): These should be dynamically generated when backend generation logic is added. -->
            <div class="todo-list">
                <todo-item
                    heading="Register product recall"
                    description="Register espresso machine for safety recall service.">
                </todo-item>
                <todo-item
                    heading="Complete security training"
                    description="Finish mandatory annual security awareness module.">
                </todo-item>
                <todo-item
                    heading="Flight check-in"
                    description="Check in for upcoming flight to SFO.">
                </todo-item>
            </div>
        </section>
    </main>
  `;
}
