// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './todo_item.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {AutoTodoGroup, browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {AutoTodoItem} from '../context_hub.mojom-webui.js';

import {getCss} from './ai_taskbox.css.js';
import {getHtml} from './ai_taskbox.html.js';

const GENERAL_FEEDBACK_FORM_URL = 'https://forms.gle/sfEC2J7QBuz6zmbD7';

function getTabTodoPriority(item: AutoTodoItem): number {
  const group = item.data.thirdParty?.groupType;

  // Unfinished action todos are the highest priority and should be shown first.
  if (group === AutoTodoGroup.kUnfinishedAction) {
    return 0;
  }
  if (group === AutoTodoGroup.kNudgeToClose) {
    return 1;
  }
  return 2;
}

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
      hasGmailGenerationError_: {type: Boolean},
      hasGeneratedGmail_: {type: Boolean},
      isGeneratingTabTodos_: {type: Boolean},
      hasTabGenerationError_: {type: Boolean},
      hasGeneratedTab_: {type: Boolean},
      autoTodosEnabled_: {type: Boolean},
    };
  }

  accessor todos: AutoTodoItem[]|null = null;
  accessor tabTodos: AutoTodoItem[]|null = null;
  protected accessor isGeneratingGmailTodos_: boolean = false;
  protected accessor hasGmailGenerationError_: boolean = false;
  protected accessor hasGeneratedGmail_: boolean = false;
  protected accessor isGeneratingTabTodos_: boolean = false;
  // TODO(crbug.com/539697847): Use this to show an error message to the user.
  protected accessor hasTabGenerationError_: boolean = false;
  protected accessor hasGeneratedTab_: boolean = false;
  protected accessor autoTodosEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTodos');
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    if (this.autoTodosEnabled_) {
      this.listenerIds_.push(
          browserProxyFactory.getInstance()
              .callbackRouter.onAutoTodosChanged.addListener(
                  (todos: AutoTodoItem[]) => {
                    this.todos = todos.filter(todo => !!todo.data.firstParty)
                                     .sort((a, b) => b.score - a.score);
                    this.tabTodos = todos.filter(todo => !!todo.data.thirdParty)
                                        .sort(
                                            (a, b) => getTabTodoPriority(a) -
                                                getTabTodoPriority(b));
                  }));
      this.fetchAutoTodos_();
    }
  }

  private async fetchAutoTodos_() {
    try {
      const {firstPartyTodos, thirdPartyTodos} =
          await browserProxyFactory.getInstance().handler.getAutoTodos();
      this.todos = firstPartyTodos.sort((a, b) => b.score - a.score) ?? null;
      this.tabTodos =
          thirdPartyTodos.sort(
              (a, b) => getTabTodoPriority(a) - getTabTodoPriority(b)) ??
          null;
    } catch (e) {
      console.error('Failed to fetch auto todos:', e);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => browserProxyFactory.getInstance().callbackRouter.removeListener(
            id));
    this.listenerIds_ = [];
  }

  protected onGeneralFeedbackClick_() {
    window.open(GENERAL_FEEDBACK_FORM_URL, '_blank');
  }

  protected async onGenerateGmailTodosClick_() {
    if (!this.autoTodosEnabled_ || this.isGeneratingGmailTodos_) {
      return;
    }
    this.isGeneratingGmailTodos_ = true;
    this.hasGmailGenerationError_ = false;
    try {
      const {success} = await browserProxyFactory.getInstance()
                            .handler.generateFirstPartyAutoTodos();
      this.hasGmailGenerationError_ = !success;
      if (success) {
        this.hasGeneratedGmail_ = true;
      }
    } catch (e) {
      console.error('Failed to generate Gmail auto todos:', e);
      this.hasGmailGenerationError_ = true;
    } finally {
      this.isGeneratingGmailTodos_ = false;
    }
  }

  protected async onGenerateTabTodosClick_() {
    if (!this.autoTodosEnabled_ || this.isGeneratingTabTodos_) {
      return;
    }
    this.isGeneratingTabTodos_ = true;
    this.hasTabGenerationError_ = false;
    try {
      const {success} = await browserProxyFactory.getInstance()
                            .handler.generateTabBasedTodos();
      this.hasTabGenerationError_ = !success;
      if (success) {
        this.hasGeneratedTab_ = true;
      }
    } catch (e) {
      console.error('Failed to generate tab-based todos:', e);
      this.hasTabGenerationError_ = true;
    } finally {
      this.isGeneratingTabTodos_ = false;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-taskbox': AiTaskboxElement;
  }
}

customElements.define(AiTaskboxElement.is, AiTaskboxElement);
