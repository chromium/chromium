// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TodoItemElement} from './todo_item.js';
import {TodoItemVariant} from './todo_item.js';

export function getHtml(this: TodoItemElement) {
  return this.variant === TodoItemVariant.TAB ?
      html`
      <div class="todo-content tab-todo-content">
        <cr-icon-button id="check-circle"
            disabled=true
            iron-icon="cr:check-circle"
            @click="${this.onCheckCircleClick_}">
        </cr-icon-button>
        <div class="todo-info">
          <h3 title="${this.heading}">${this.heading}</h3>
          <p class="description">${this.description}</p>
        </div>
        <div class="todo-actions" @click="${this.onActionsClick_}">
          <cr-button class="tonal-button" disabled=true
              @click="${this.onOpenTabClick_}">Open tab</cr-button>
          <div class="thumb-group">
            <cr-icon-button id="thumbsUp" disabled=true
                iron-icon="${this.getThumbsUpIcon_()}"
                title="Like"
                aria-pressed="${this.liked === true}"
                @click="${this.onThumbsUpClick_}">
            </cr-icon-button>
            <cr-icon-button id="thumbsDown" disabled=true
                iron-icon="${this.getThumbsDownIcon_()}"
                title="Dislike"
                aria-pressed="${this.liked === false}"
                @click="${this.onThumbsDownClick_}">
            </cr-icon-button>
          </div>
          <cr-icon-button id="moreButton"
              iron-icon="cr:more-vert"
              title="More actions"
              @click="${this.onMoreClick_}">
          </cr-icon-button>
        </div>
      </div>
      <cr-action-menu id="menu">
        <button class="dropdown-item" @click="${this.onCloseTabClick_}">
          Close tab
        </button>
        <button class="dropdown-item" @click="${this.onSaveClick_}">
          Save for later
        </button>
      </cr-action-menu>
    ` :
      html`
    <cr-expand-button
        ?expanded="${this.expanded_}"
        @expanded-changed="${this.onExpandedChanged_}">
      <div class="todo-content">
        <cr-icon-button id="check-circle"
            disabled=true
            iron-icon="cr:check-circle"
            @click="${this.onCheckCircleClick_}">
        </cr-icon-button>
        <div class="todo-info">
          <h3>${this.heading}</h3>
          <p class="description">${this.description}</p>
        </div>
        <div class="todo-actions" @click="${this.onActionsClick_}">
          <cr-button class="tonal-button"
              @click="${this.onOpenTabClick_}">Open tab</cr-button>
          <div class="thumb-group">
            <cr-icon-button id="thumbsUp"
                iron-icon="${this.getThumbsUpIcon_()}"
                title="Like"
                aria-pressed="${this.liked === true}"
                @click="${this.onThumbsUpClick_}">
            </cr-icon-button>
            <cr-icon-button id="thumbsDown"
                iron-icon="${this.getThumbsDownIcon_()}"
                title="Dislike"
                aria-pressed="${this.liked === false}"
                @click="${this.onThumbsDownClick_}">
            </cr-icon-button>
          </div>
        </div>
      </div>
    </cr-expand-button>
    ${
          this.expanded_ ? html`
      <div class="expanded-content">
        <div class="expanded-details">
          <div>Score: ${this.score?.toFixed(2)}</div>
          <div class="references-section">
            <span class="references-label">From:</span>
            <div class="references-list">
              ${
                               this.getReferences().map(
                                   ref => html`
                <a class="reference-link" href="${
                                       ref.url}" target="_blank" title="${
                                       ref.label}">${ref.label}</a>
              `)}
            </div>
          </div>
        </div>
        <cr-button disabled=true class="dismiss-button" @click="${
                               this.onDismissClick_}">
          Dismiss Todo
        </cr-button>
      </div>
    ` :
                           ''}
  `;
}
