// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {AiTaskboxElement} from './ai_taskbox.js';
import {TodoItemVariant} from './todo_item.js';

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
                    <h2>Workspace Todos</h2>
                    <cr-button class="tonal-button"
                        ?disabled="${!this.autoTodosEnabled_ || this.isGeneratingGmailTodos_}"
                        @click="${this.onGenerateGmailTodosClick_}">
                      ${this.isGeneratingGmailTodos_ ? 'Generating...' : 'Generate Workspace Todos'}
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
                              this.hasGmailGenerationError_ ? html`
                      <div class="placeholder-card">
                        <p class="placeholder-text error-text">Failed to generate. Please try again.</p>
                      </div>
                    ` : this.hasGeneratedGmail_ ? html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">You're all caught up!</p>
                      </div>
                    ` : html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">No Workspace Todos yet.</p>
                      </div>
                    `}
                </div>
            </section>

            <!-- Tab-based Todos Section -->
            <section class="todo-column">
                <div class="column-header">
                    <h2>Browser Todos</h2>
                    <cr-button class="tonal-button" disabled
                        @click="${this.onGenerateTabTodosClick_}">
                      ${this.isGeneratingTabTodos_ ? 'Generating...' : 'Generate Browser Todos'}
                    </cr-button>
                </div>

                <div class="todo-list">
                    ${
      this.tabTodos &&
      this.tabTodos.length > 0 ? this.tabTodos.map(todo => html`
                      <todo-item
                          .id="${todo.id}"
                          .heading="${todo.title}"
                          .description="${todo.description}"
                          .variant="${TodoItemVariant.TAB}">
                      </todo-item>
                    `) :
      this.hasTabGenerationError_ ? html`
                      <div class="placeholder-card">
                        <p class="placeholder-text error-text">Failed to generate. Please try again.</p>
                      </div>
                    ` : this.hasGeneratedTab_ ? html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">You're all caught up!</p>
                      </div>
                    ` : html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">No Browser Todos yet.</p>
                      </div>
                    `}
                </div>
            </section>
        </div>
    </main>
  `;
}
