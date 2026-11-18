// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {AutoTodoStatus} from '../context_hub.mojom-webui.js';

import type {TodoItemElement} from './todo_item.js';

export function getHtml(this: TodoItemElement) {
  return html`
<cr-expand-button ?expanded="${this.expanded_}"
    @expanded-changed="${this.onExpandedChanged_}">
  <div class="todo-content">
    <div class="todo-main">
      ${this.status === AutoTodoStatus.kDismissed ? html`
        <cr-button class="tonal-button mark-active-button" id="markActiveButton"
            ?disabled="${this.disable_state_mgmt}"
            @click="${this.onMarkActiveClick_}">
          Mark active
        </cr-button>
      ` : html`
        <cr-icon-button id="check-circle" ?disabled="${this.disable_state_mgmt}"
            iron-icon="cr:check-circle" @click="${this.onCheckCircleClick_}">
        </cr-icon-button>
      `}
      <div class="todo-info">
        <h3>${this.heading}</h3>
        <p class="description">${this.description}</p>
      </div>
    </div>
    <div class="todo-actions" @click="${this.onActionsClick_}">
      ${this.canActOnWeb_ ? html`
        <div class="split-button">
          <cr-button class="tonal-button split-main-button" id="openTabButton"
              ?disabled="${!this.canOpenTab_()}"
              @click="${this.onOpenTabClick_}">
            Open tab
          </cr-button>
          <cr-button class="tonal-button split-dropdown-button"
              id="taskDropdownButton" aria-label="More task actions"
              title="More task actions" aria-haspopup="menu"
              @click="${this.onTaskMenuClick_}">
            <cr-icon icon="cr:arrow-drop-down"></cr-icon>
          </cr-button>
        </div>
      ` : html`
        <cr-button class="tonal-button" id="openTabButton"
            ?disabled="${!this.canOpenTab_()}" @click="${this.onOpenTabClick_}">
          Open tab
        </cr-button>
      `}
      <div class="thumb-group">
        <cr-icon-button id="thumbsUp" iron-icon="${this.getThumbsUpIcon_()}"
            title="Like" aria-pressed="${this.liked === true}"
            @click="${this.onThumbsUpClick_}">
        </cr-icon-button>
        <cr-icon-button id="thumbsDown" iron-icon="${this.getThumbsDownIcon_()}"
            title="Dislike" aria-pressed="${this.liked === false}"
            @click="${this.onThumbsDownClick_}">
        </cr-icon-button>
      </div>
    </div>
  </div>
</cr-expand-button>
${this.canActOnWeb_ ? html`
  <cr-action-menu id="taskMenu">
    <button class="dropdown-item" ?disabled="${!this.canOpenTab_()}"
        @click="${this.onOpenTabClick_}">
      Open tab
    </button>
    <button class="dropdown-item" @click="${this.onRunTaskClick_}">
      Run task
    </button>
  </cr-action-menu>
` : ''}
${this.expanded_ ? html`
  <div class="expanded-content">
    <div class="expanded-details">
      <div>Score: ${this.score?.toFixed(2)}</div>
      <div class="references-section">
        <span class="references-label">From:</span>
        <div class="references-list">
          ${this.getReferences().map(ref => html`
            <a class="reference-link" href="${ref.url}" target="_blank"
                title="${ref.label}">
              ${ref.label}
            </a>
          `)}
        </div>
      </div>
    </div>
    ${this.status !== AutoTodoStatus.kDismissed ? html`
      <cr-button class="dismiss-button" ?disabled="${this.disable_state_mgmt}"
          @click="${this.onDismissClick_}">
        Dismiss Todo
      </cr-button>
    ` : ''}
  </div>
` : ''}`;
}
