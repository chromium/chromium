// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import type {AiTaskboxElement} from './ai_taskbox.js';
import {TodoItemVariant} from './todo_item.js';

export function getHtml(this: AiTaskboxElement) {
  return this.showingReadingList_ ? html`
    <main id="reading-list-view" @feedback-changed="${this.onFeedbackChanged_}">
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
                        .liked="${this.feedbacks_.get(todo.id) ?? null}"
                        .disable_state_mgmt="${this.isGeneratingTabTodos_}">
                    </todo-item>
                  `) :
          ''}
      </div>
    </main>
  ` : html`
    <main id="dashboard-view" @feedback-changed="${this.onFeedbackChanged_}">
        <section class="header-section">
            <!-- TODO(crbug.com/519576944): Replace with the dynamic greeting title. -->
            <h1>LaunchPad</h1>
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
                    <div class="refresh-container">
                      <span class="last-updated-text">
                        ${
          this.isGeneratingGmailTodos_ ?
              'Generating...' :
              this.getFormattedTimeAgo_(this.lastGmailGenerationTime_)}
                      </span>
                      <cr-icon-button
                          iron-icon="cr:sync"
                          title="Refresh"
                          aria-label="Refresh Workspace Todos"
                          ?disabled="${
          !this.autoTodosEnabled_ || this.isGeneratingGmailTodos_}"
                          @click="${this.onGenerateGmailTodosClick_}">
                      </cr-icon-button>
                      <cr-icon-button
                          id="workspaceDropdownButton"
                          iron-icon="cr:arrow-drop-down"
                          title="clears all todos and any context. regeneration after clearing may result in previously dismissed/cleared todos"
                          aria-label="Clear ALL Workspace Todos"
                          ?disabled="${
          !this.autoTodosEnabled_ || this.isGeneratingGmailTodos_}"
                          @click="${this.onWorkspaceMenuClick_}">
                      </cr-icon-button>
                    </div>
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
                          .liked="${this.feedbacks_.get(todo.id) ?? null}"
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
                                  .liked="${this.feedbacks_.get(todo.id) ?? null}"
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
                    <div class="refresh-container">
                      <span class="last-updated-text">
                        ${
          this.isGeneratingTabTodos_ ?
              'Generating...' :
              this.getFormattedTimeAgo_(this.lastTabGenerationTime_)}
                      </span>
                      <cr-icon-button
                          iron-icon="cr:sync"
                          title="Refresh"
                          aria-label="Refresh Browser Todos"
                          ?disabled="${
          !this.autoTodosEnabled_ || this.isGeneratingTabTodos_}"
                          @click="${this.onGenerateTabTodosClick_}">
                      </cr-icon-button>
                      <cr-icon-button
                          id="browserDropdownButton"
                          iron-icon="cr:arrow-drop-down"
                          title="clears all todos and any context. regeneration after clearing may result in previously dismissed/cleared todos"
                          aria-label="Clear ALL Browser Todos"
                          ?disabled="${
          !this.autoTodosEnabled_ || this.isGeneratingTabTodos_}"
                          @click="${this.onBrowserMenuClick_}">
                      </cr-icon-button>
                    </div>
                </div>

                ${this.getUnfinishedTabTodos_().length > 0 || this.getShoppingCartTabTodos_().length > 0 || this.getStaleTabTodos_().length > 0 ? html`
                  <div class="category-sections">
                    ${this.getUnfinishedTabTodos_().length > 0 ? html`
                      <div class="category-section">
                        <div class="category-header">
                          <h3>Pending actions</h3>
                        </div>
                        <div class="todo-list">
                          ${repeat(this.getUnfinishedTabTodos_(), todo => todo.id, todo => html`
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
                                .liked="${this.feedbacks_.get(todo.id) ?? null}"
                                .disable_state_mgmt="${this.isGeneratingTabTodos_}">
                            </todo-item>
                          `)}
                        </div>
                      </div>
                    ` : ''}

                    ${this.getShoppingCartTabTodos_().length > 0 ? html`
                      <div class="category-section">
                        <div class="category-header">
                          <h3>Shopping carts</h3>
                        </div>
                        <div class="todo-list">
                          ${repeat(this.getShoppingCartTabTodos_(), todo => todo.id, todo => html`
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
                                .liked="${this.feedbacks_.get(todo.id) ?? null}"
                                .disable_state_mgmt="${this.isGeneratingTabTodos_}">
                            </todo-item>
                          `)}
                        </div>
                      </div>
                    ` : ''}

                    ${this.getStaleTabTodos_().length > 0 ? html`
                      <div class="category-section">
                        <div class="category-header">
                          <h3>Stale tabs</h3>
                          <cr-button class="tonal-button"
                              ?disabled="${this.isGeneratingTabTodos_}"
                              @click="${this.onCloseAllStaleTabsClick_}">
                            Close all tabs
                          </cr-button>
                        </div>
                        <div class="todo-list">
                          ${repeat(this.getStaleTabTodos_(), todo => todo.id, todo => html`
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
                                .liked="${this.feedbacks_.get(todo.id) ?? null}"
                                .disable_state_mgmt="${this.isGeneratingTabTodos_}">
                            </todo-item>
                          `)}
                        </div>
                      </div>
                    ` : ''}
                  </div>
                ` : html`
                  <div class="todo-list">
                    ${this.hasTabGenerationError_ ? html`
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
                `}

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
                                  .liked="${this.feedbacks_.get(todo.id) ?? null}"
                                  .disable_state_mgmt="${this.isGeneratingTabTodos_}">
                              </todo-item>
                            `) : ''}
                        </div>
                    </cr-collapse>
                </div>
            </section>
        </div>

        <cr-action-menu id="workspaceMenu">
          <button class="dropdown-item"
              title="clears all todos and any context. regeneration after clearing may result in previously dismissed/cleared todos"
              @click="${this.onClearWorkspaceTodosClick_}">
            Clear ALL Workspace Todos
          </button>
        </cr-action-menu>

        <cr-action-menu id="browserMenu">
          <button class="dropdown-item"
              title="clears all todos and any context. regeneration after clearing may result in previously dismissed/cleared todos"
              @click="${this.onClearBrowserTodosClick_}">
            Clear ALL Browser Todos
          </button>
        </cr-action-menu>
    </main>
  `;
}
