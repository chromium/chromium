// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {AutoTodoStatus} from '../context_hub.mojom-webui.js';

import type {TodoItemElement} from './todo_item.js';

export function getHtml(this: TodoItemElement) {
  return html`
<div class="todo-content">
  <div class="todo-main">
    ${this.status !== AutoTodoStatus.kDismissed ? html`
      <cr-icon-button id="check-circle"
          class="no-overlap${
              this.status === AutoTodoStatus.kCompleted ? ' completed' : ''}"
          ?disabled="${this.disable_state_mgmt}"
          iron-icon="${this.getCheckCircleIcon_()}"
          title="${this.status === AutoTodoStatus.kCompleted ?
              'Mark active' :
              'Mark completed'}"
          aria-label="${this.status === AutoTodoStatus.kCompleted ?
              'Mark active' :
              'Mark completed'}"
          @click="${this.onCheckCircleClick_}">
      </cr-icon-button>
    ` : ''}
    <div class="todo-info">
      <h3>${this.heading}</h3>
      <p class="description">${this.description}</p>
      ${this.references_.length > 0 ||
              (this.feedbackMode && this.score != null) ?
          html`
        <div class="todo-metadata">
          ${this.references_.length > 0 ? html`
            <div class="references-section">
              <span class="references-label">From:</span>
              <div class="references-list">
                ${this.references_.map(item => html`
                  <a class="reference-link" href="${item.url}" target="_blank"
                      title="${item.label}">
                    ${item.label}
                  </a>
                `)}
              </div>
            </div>
          ` : ''}
          ${this.feedbackMode && this.score != null ? html`
            <span class="score-badge">Score: ${this.score.toFixed(2)}</span>
          ` : ''}
        </div>
      ` : ''}
    </div>
  </div>
  <div class="todo-actions">
    ${this.canActOnWeb_ ? html`
      <cr-button class="tonal-button run-task-button" id="runTaskButton"
          title="Run task" aria-label="Run task"
          ?disabled="${this.disable_state_mgmt || this.isRunningTask_ ||
              !this.canRunTask_()}"
          @click="${this.onRunTaskButtonClick_}">
        <cr-icon icon="context-hub:cursor-spark"></cr-icon>
      </cr-button>
    ` : ''}
    <cr-button class="tonal-button action-pill-button" id="openTabButton"
        ?disabled="${!this.canOpenTab_()}" @click="${this.onOpenTabClick_}">
      Open tab
    </cr-button>
    ${this.feedbackMode ? html`
      <div class="thumb-group">
        <cr-icon-button class="no-overlap" id="thumbsUp"
            iron-icon="${this.getThumbsUpIcon_()}" title="Like"
            aria-pressed="${this.liked === true}"
            ?disabled="${this.disable_feedback}"
            @click="${this.onThumbsUpClick_}">
        </cr-icon-button>
        <cr-icon-button class="no-overlap" id="thumbsDown"
            iron-icon="${this.getThumbsDownIcon_()}" title="Dislike"
            aria-pressed="${this.liked === false}"
            ?disabled="${this.disable_feedback}"
            @click="${this.onThumbsDownClick_}">
        </cr-icon-button>
      </div>
    ` : ''}
    <cr-icon-button class="no-overlap" id="moreButton" iron-icon="cr:more-vert"
        title="More actions" aria-label="More actions" aria-haspopup="menu"
        ?disabled="${this.disable_state_mgmt}"
        @click="${this.onMoreButtonClick_}">
    </cr-icon-button>
  </div>
</div>
<cr-action-menu id="menu">
  ${this.status === AutoTodoStatus.kDismissed ? html`
    <button class="dropdown-item" id="menuMarkActiveButton"
        ?disabled="${this.disable_state_mgmt}"
        @click="${this.onMenuMarkActiveButtonClick_}">
      Mark active
    </button>
  ` : html`
    <button class="dropdown-item" id="dismissButton"
        ?disabled="${this.disable_state_mgmt}"
        @click="${this.onDismissButtonClick_}">
      Dismiss Todo
    </button>
  `}
</cr-action-menu>`;
}
