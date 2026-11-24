// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import type {AiTaskboxElement} from './ai_taskbox.js';

export function getHtml(this: AiTaskboxElement) {
  return html`
<main id="dashboard-view" @feedback-changed="${this.onFeedbackChanged_}">
  <div class="launchpad-container">
    <section class="header-section">
      <!-- TODO(crbug.com/519576944): Replace with the dynamic greeting
           title. -->
      <h1>
        You have
        <span class="todo-count">
          ${this.todos?.length || 0}
          ${(this.todos?.length || 0) === 1 ? 'to-do' : 'to-dos'}
        </span>
        to catch up on
      </h1>
      <div class="header-buttons">
        <div class="feedback-toggle-container">
          <span id="feedbackModeLabel" class="feedback-toggle-label">
            Feedback mode
          </span>
          <cr-toggle id="feedbackModeToggle"
              ?checked="${this.feedbackModeEnabled_}"
              aria-labelledby="feedbackModeLabel"
              @change="${this.onFeedbackModeToggleChange_}">
          </cr-toggle>
        </div>
        ${this.feedbackModeEnabled_ ? html`
          <cr-button id="generalFeedbackButton"
              @click="${this.onGeneralFeedbackClick_}">
            General Feedback Form
          </cr-button>
        ` : ''}
      </div>
    </section>

    <section class="todo-column">
      <div class="column-header">
        <h2>To-dos</h2>
        <div class="refresh-container">
          <span class="last-updated-text">
            ${this.isGeneratingGmailTodos_ ?
                'Generating...' :
                this.hasGmailGenerationError_ ?
                'Failed to generate' :
                this.getFormattedTimeAgo_(this.lastGmailGenerationTime_)}
          </span>
          <cr-icon-button id="refreshButton" iron-icon="cr:sync" title="Refresh"
              aria-label="Refresh Todos" ?disabled="${
                  (!this.launchpadEnabled_ && !this.autoTodosEnabled_) ||
                  this.isGeneratingGmailTodos_}"
              @click="${this.onGenerateGmailTodosClick_}">
          </cr-icon-button>
          ${!this.launchpadEnabled_ ? html`
            <cr-icon-button id="workspaceDropdownButton"
                iron-icon="cr:arrow-drop-down"
                title="Clears all todos and context"
                aria-label="Clear ALL Todos" ?disabled="${
                    !this.autoTodosEnabled_ || this.isGeneratingGmailTodos_}"
                @click="${this.onWorkspaceMenuClick_}">
            </cr-icon-button>
          ` : ''}
        </div>
      </div>

      <div class="todo-list">
        ${this.todos && this.todos.length > 0 ? html`
          ${repeat(this.todos, item => item.id, item => html`
            <todo-item .id="${item.id}" .heading="${item.title}"
                .description="${item.description}" .status="${item.status}"
                .actionableUrl="${item.actionableUrl || ''}"
                .sourceReferences="${item.sourceReferences}"
                .score="${item.score}"
                .liked="${this.feedbacks_.get(item.id) ?? null}"
                .feedbackMode="${this.feedbackModeEnabled_}"
                .disable_state_mgmt="${
                    this.launchpadEnabled_ || this.isGeneratingGmailTodos_}"
                .disable_feedback="${this.launchpadEnabled_}">
            </todo-item>
          `)}
        ` : html`
          ${this.hasGmailGenerationError_ ? html`
            <div class="placeholder-card">
              <p class="placeholder-text error-text">
                Failed to generate. Please try again.
              </p>
            </div>
          ` : html`
            ${this.hasGeneratedGmail_ ? html`
              <div class="placeholder-card">
                <p class="placeholder-text">You're all caught up!</p>
              </div>
            ` : html`
              <div class="placeholder-card">
                <p class="placeholder-text">No To-dos yet.</p>
              </div>
            `}
          `}
        `}
      </div>

      <!-- Completed Todos Section -->
      <div class="completed-section">
        <cr-expand-button class="completed-expand-button"
            ?disabled="${(this.completedTodos?.length || 0) === 0}"
            ?expanded="${this.isCompletedExpanded_ &&
                (this.completedTodos?.length || 0) > 0}"
            @expanded-changed="${this.onCompletedExpandedChanged_}" no-hover>
          <h2>
            Completed To-dos (${this.completedTodos?.length || 0})
          </h2>
        </cr-expand-button>

        <cr-collapse
            ?opened="${this.isCompletedExpanded_ &&
                (this.completedTodos?.length || 0) > 0}">
          <div class="todo-list completed-todo-list">
            ${this.completedTodos && this.completedTodos.length > 0 ? html`
              ${repeat(this.completedTodos, item => item.id, item => html`
                <todo-item .id="${item.id}" .heading="${item.title}"
                    .description="${item.description}" .status="${item.status}"
                    .actionableUrl="${item.actionableUrl || ''}"
                    .sourceReferences="${item.sourceReferences}"
                    .score="${item.score}"
                    .liked="${this.feedbacks_.get(item.id) ?? null}"
                    .feedbackMode="${this.feedbackModeEnabled_}"
                    .disable_state_mgmt="${this.launchpadEnabled_ ||
                        this.isGeneratingGmailTodos_}"
                    .disable_feedback="${this.launchpadEnabled_}">
                </todo-item>
              `)}
            ` : ''}
          </div>
        </cr-collapse>
      </div>

      <!-- Dismissed Todos Section -->
      <div class="dismissed-section">
        <cr-expand-button class="dismissed-expand-button"
            ?disabled="${(this.dismissedTodos?.length || 0) === 0}"
            ?expanded="${this.isDismissedExpanded_ &&
                (this.dismissedTodos?.length || 0) > 0}"
            @expanded-changed="${this.onDismissedExpandedChanged_}" no-hover>
          <h2>
            Dismissed To-dos (${this.dismissedTodos?.length || 0})
          </h2>
        </cr-expand-button>

        <cr-collapse
            ?opened="${this.isDismissedExpanded_ &&
                (this.dismissedTodos?.length || 0) > 0}">
          <div class="todo-list dismissed-todo-list">
            ${this.dismissedTodos && this.dismissedTodos.length > 0 ? html`
              ${repeat(this.dismissedTodos, item => item.id, item => html`
                <todo-item .id="${item.id}" .heading="${item.title}"
                    .description="${item.description}" .status="${item.status}"
                    .actionableUrl="${item.actionableUrl || ''}"
                    .sourceReferences="${item.sourceReferences}"
                    .score="${item.score}"
                    .liked="${this.feedbacks_.get(item.id) ?? null}"
                    .feedbackMode="${this.feedbackModeEnabled_}"
                    .disable_state_mgmt="${this.launchpadEnabled_ ||
                        this.isGeneratingGmailTodos_}"
                    .disable_feedback="${this.launchpadEnabled_}">
                </todo-item>
              `)}
            ` : ''}
          </div>
        </cr-collapse>
      </div>
    </section>

    ${!this.launchpadEnabled_ ? html`
      <cr-action-menu id="workspaceMenu">
        <button class="dropdown-item" title="${this.clearAllTodosTooltip_}"
            @click="${this.onClearWorkspaceTodosClick_}">
          Clear ALL Todos
        </button>
      </cr-action-menu>
    ` : ''}
  </div>
</main>`;
}
