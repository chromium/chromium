// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './todo_item.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {AutoTodoItem} from '../context_hub.mojom-webui.js';

import {getCss} from './ai_taskbox.css.js';
import {getHtml} from './ai_taskbox.html.js';

const GENERAL_FEEDBACK_FORM_URL = 'https://forms.gle/sfEC2J7QBuz6zmbD7';

export class AiTaskboxElement extends CrLitElement {
  static get is() {
    return 'ai-taskbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      todos: {type: Array},
      tabTodos: {type: Array},
      isGeneratingGmailTodos_: {type: Boolean},
      autoTodosEnabled_: {type: Boolean},
    };
  }

  accessor todos: AutoTodoItem[]|null = null;
  accessor tabTodos: AutoTodoItem[]|null = null;
  protected accessor isGeneratingGmailTodos_: boolean = false;
  protected accessor autoTodosEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTodos');

  protected onGeneralFeedbackClick_() {
    window.open(GENERAL_FEEDBACK_FORM_URL, '_blank');
  }

  protected async onGenerateGmailTodosClick_() {
    if (!this.autoTodosEnabled_ || this.isGeneratingGmailTodos_) {
      return;
    }
    this.isGeneratingGmailTodos_ = true;
    try {
      const {todos} =
          await browserProxyFactory.getInstance().handler.generateAutoTodos();
      this.todos = todos;
    } finally {
      this.isGeneratingGmailTodos_ = false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-taskbox': AiTaskboxElement;
  }
}

customElements.define(AiTaskboxElement.is, AiTaskboxElement);
