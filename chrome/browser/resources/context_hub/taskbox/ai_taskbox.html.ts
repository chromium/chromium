// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import type {AiTaskboxElement} from './ai_taskbox.js';
import {TodoItemVariant} from './todo_item.js';

export function getHtml(this: AiTaskboxElement) {
  return this.showingReadingList_ ? html`
    <main id="reading-list-view">
      <section class="header-section">
        <div class="header-title-container">
          <cr-icon-button
              id="back-button"
              iron-icon="cr:arrow-back"
              aria-label="Back"
              @click="${this.onBackClick_}">
          </cr-icon-button>
          <h1>Reading List</h1>
        </div>
      </section>

      <div class="todo-list">
        ${
      this.readingListTodos ?
          repeat(
              this.readingListTodos, todo => todo.id,
              todo => html`
                    <todo-item
                        .id="${todo.id}"
                        .heading="${todo.title}"
                        .description="${todo.description}"
                        .status="${todo.status}"
                        .tabId="${todo.data.thirdParty!.tabId}"
                        .lastActiveTimestamp="${
                  todo.data.thirdParty!.lastActiveTimestamp}"
                        .groupType="${todo.data.thirdParty!.groupType}"
                        .variant="${TodoItemVariant.TAB}"
                        .disable_state_mgmt="${this.isGeneratingTabTodos_}">
                    </todo-item>
                  `) :
          ''}
      </div>
    </main>
  ` : html`
    <main id="dashboard-view">
        <section class="header-section">
            <!-- TODO(crbug.com/519576944): Replace with the dynamic greeting title. -->
            <h1>AI Taskbox</h1>
            <div class="header-buttons">
              <cr-button
                  ?disabled="${(this.readingListTodos?.length || 0) === 0}"
                  @click="${this.onGoToReadingListClick_}">
                My Reading List (${this.readingListTodos?.length || 0})
              </cr-button>
              <cr-button @click="${this.onGeneralFeedbackClick_}">
                General Feedback Form
              </cr-button>
            </div>
        </section>

        <div class="columns-container">
            <!-- Gmail Todos Section -->
            <section class="todo-column">
                <div class="column-header">
                    <h2>Workspace Todos</h2>
                    <cr-button class="tonal-button"
                        ?disabled="${
      !this.autoTodosEnabled_ || this.isGeneratingGmailTodos_}"
                        @click="${this.onGenerateGmailTodosClick_}">
                      ${
      this.isGeneratingGmailTodos_ ? 'Generating...' :
                                     'Generate Workspace Todos'}
                    </cr-button>
                </div>

                <div class="todo-list">
                    ${
      this.todos && this.todos.length > 0 ?
          repeat(
              this.todos, todo => todo.id,
              todo => html`
                      <todo-item
                          .id="${todo.id}"
                          .heading="${todo.title}"
                          .description="${todo.description}"
                          .status="${todo.status}"
                          .actionableUrl="${
                  todo.data.firstParty?.actionableUrl || ''}"
                          .sourceReferences="${
                  todo.data.firstParty?.sourceReferences || []}"
                          .score="${todo.score}"
                          .disable_state_mgmt="${this.isGeneratingGmailTodos_}">
                      </todo-item>
                    `) :
          this.hasGmailGenerationError_ ? html`
                      <div class="placeholder-card">
                        <p class="placeholder-text error-text">Failed to generate. Please try again.</p>
                      </div>
                    ` :
          this.hasGeneratedGmail_       ? html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">You're all caught up!</p>
                      </div>
                    ` :
                                          html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">No Workspace Todos yet.</p>
                      </div>
                    `}
                </div>

                <!-- Completed Workspace Todos Section -->
                <div class="completed-section">
                    <cr-expand-button
                        class="completed-expand-button"
                        ?disabled="${(this.completedTodos?.length || 0) === 0}"
                        ?expanded="${this.isCompletedExpanded_ && (this.completedTodos?.length || 0) > 0}"
                        @expanded-changed="${this.onCompletedExpandedChanged_}"
                        no-hover>
                        <h2>Completed Workspace Todos (${this.completedTodos?.length || 0})</h2>
                    </cr-expand-button>

                    <cr-collapse ?opened="${this.isCompletedExpanded_ && (this.completedTodos?.length || 0) > 0}">
                        <div class="todo-list completed-todo-list">
                            ${
      this.completedTodos &&
      this.completedTodos.length > 0 ? repeat(this.completedTodos, todo => todo.id, todo => html`
                              <todo-item
                                  .id="${todo.id}"
                                  .heading="${todo.title}"
                                  .description="${todo.description}"
                                  .status="${todo.status}"
                                  .actionableUrl="${
                          todo.data.firstParty?.actionableUrl || ''}"
                                  .sourceReferences="${
                          todo.data.firstParty?.sourceReferences || []}"
                                  .score="${todo.score}"
                                  .disable_state_mgmt="${this.isGeneratingGmailTodos_}">
                              </todo-item>
                            `) : ''}
                        </div>
                    </cr-collapse>
                </div>
            </section>

            <!-- Tab-based Todos Section -->
            <section class="todo-column">
                <div class="column-header">
                    <h2>Browser Todos</h2>
                    <cr-button class="tonal-button" disabled
                        @click="${this.onGenerateTabTodosClick_}">
                      ${
      this.isGeneratingTabTodos_ ? 'Generating...' : 'Generate Browser Todos'}
                    </cr-button>
                </div>

                <div class="todo-list">
                    ${
      this.tabTodos && this.tabTodos.length > 0 ?
          repeat(this.tabTodos, todo => todo.id, todo => html`
                      <todo-item
                          .id="${todo.id}"
                          .heading="${todo.title}"
                          .description="${todo.description}"
                          .status="${todo.status}"
                          .tabId="${todo.data.thirdParty!.tabId}"
                          .lastActiveTimestamp="${
                 todo.data.thirdParty!.lastActiveTimestamp}"
                          .groupType="${todo.data.thirdParty!.groupType}"
                          .variant="${TodoItemVariant.TAB}"
                          .disable_state_mgmt="${this.isGeneratingTabTodos_}">
                      </todo-item>
                    `) :
          this.hasTabGenerationError_ ? html`
                      <div class="placeholder-card">
                        <p class="placeholder-text error-text">Failed to generate. Please try again.</p>
                      </div>
                    ` :
          this.hasGeneratedTab_       ? html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">You're all caught up!</p>
                      </div>
                    ` :
                                        html`
                      <div class="placeholder-card">
                        <p class="placeholder-text">No Browser Todos yet.</p>
                      </div>
                    `}
                </div>

                <!-- Completed Browser Todos Section -->
                <div class="completed-section">
                    <cr-expand-button
                        class="completed-expand-button"
                        ?disabled="${(this.completedTabTodos?.length || 0) === 0}"
                        ?expanded="${this.isCompletedTabExpanded_ && (this.completedTabTodos?.length || 0) > 0}"
                        @expanded-changed="${this.onCompletedTabExpandedChanged_}"
                        no-hover>
                        <h2>Completed Browser Todos (${this.completedTabTodos?.length || 0})</h2>
                    </cr-expand-button>

                    <cr-collapse ?opened="${this.isCompletedTabExpanded_ && (this.completedTabTodos?.length || 0) > 0}">
                        <div class="todo-list completed-todo-list">
                            ${
      this.completedTabTodos &&
      this.completedTabTodos.length > 0 ? repeat(this.completedTabTodos, todo => todo.id, todo => html`
                              <todo-item
                                  .id="${todo.id}"
                                  .heading="${todo.title}"
                                  .description="${todo.description}"
                                  .tabId="${todo.data.thirdParty!.tabId}"
                                  .lastActiveTimestamp="${
                          todo.data.thirdParty!.lastActiveTimestamp}"
                                  .groupType="${todo.data.thirdParty!.groupType}"
                                  .status="${todo.status}"
                                  .variant="${TodoItemVariant.TAB}"
                                  .disable_state_mgmt="${this.isGeneratingTabTodos_}">
                              </todo-item>
                            `) : ''}
                        </div>
                    </cr-collapse>
                </div>
            </section>
        </div>
    </main>
  `;
}
