// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {AiTaskboxElement} from './ai_taskbox.js';

export function getHtml(this: AiTaskboxElement) {
  return html`
    <main id="dashboard-view">
        <section class="header-section">
            <!-- TODO(crbug.com/519576944): Replace with the dynamic greeting title. -->
            <h1>AI Taskbox</h1>
            <cr-button @click="${this.onGeneralFeedbackClick_}">
              General Feedback Form
            </cr-button>
        </section>

        <div class="columns-container">
            <!-- Gmail Todos Section -->
            <section class="todo-column">
                <div class="column-header">
                    <h2>Gmail Todos</h2>
                    <cr-button class="tonal-button"
                        ?disabled="${!this.autoTodosEnabled_ || this.isGeneratingGmailTodos_}"
                        @click="${this.onGenerateGmailTodosClick_}">
                      ${this.isGeneratingGmailTodos_ ? 'Generating...' : 'Generate my Gmail Todos'}
                    </cr-button>
                </div>

                <div class="todo-list">
                    ${
      this.todos &&
      this.todos.length > 0 ? this.todos.map(todo => html`
                      <todo-item
                          .id="${todo.id}"
                          .heading="${todo.title}"
                          .description="${todo.description}"
                          .actionableUrl="${
                  todo.data.firstParty?.actionableUrl || ''}"
                          .sourceReferences="${
                  todo.data.firstParty?.sourceReferences || []}"
                          .score="${todo.score}">
                      </todo-item>
                    `) :
                              html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">No Gmail Todos yet.</p>
                      </div>
                    `}
                </div>
            </section>

            <!-- Tab-based Todos Section -->
            <section class="todo-column">
                <div class="column-header">
                    <h2>Tab-based Todos</h2>
                    <cr-button class="tonal-button" disabled>
                      Generate my Tab Todos
                    </cr-button>
                </div>

                <div class="todo-list">
                    ${
      this.tabTodos &&
      this.tabTodos.length > 0 ? this.tabTodos.map(todo => html`
                      <!-- TODO(crbug.com/539697023): Update properties once tab-based todos are implemented. -->
                      <todo-item
                          .id="${todo.id}"
                          .heading="${todo.title}"
                          .description="${todo.description}"
                          .actionableUrl="${
                  todo.data.firstParty?.actionableUrl || ''}"
                          .sourceReferences="${
                  todo.data.firstParty?.sourceReferences || []}"
                          .score="${todo.score}">
                      </todo-item>
                    `) :
                                 html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">No Tab-based Todos yet.</p>
                      </div>
                    `}
                </div>
            </section>
        </div>
    </main>
  `;
}
