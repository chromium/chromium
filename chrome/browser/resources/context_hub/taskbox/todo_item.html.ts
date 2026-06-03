// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TodoItemElement} from './todo_item.js';

export function getHtml(this: TodoItemElement) {
  return html`
    <cr-expand-button
        ?expanded="${this.expanded_}"
        @expanded-changed="${this.onExpandedChanged_}">
      <div class="todo-content">
        <div class="todo-info">
          <h3>${this.heading}</h3>
          <p class="description">${this.description}</p>
        </div>
        <div class="todo-actions" @click="${this.onActionsClick_}">
          <cr-button>
            Start <cr-icon icon="cr:arrow-drop-down"></cr-icon>
          </cr-button>
          <cr-icon-button iron-icon="cr:more-vert" title="More options"></cr-icon-button>
        </div>
      </div>
    </cr-expand-button>
  `;
}
