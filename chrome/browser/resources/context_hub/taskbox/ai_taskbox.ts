// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './todo_item.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {AutoTodoGroup, AutoTodoStatus, browserProxyFactory} from '../context_hub.mojom-webui.js';
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
      autoTodosEnabled_: {type: Boolean},
      showingReadingList_: {type: Boolean},
      // Gmail-based todo properties.
      todos: {type: Array},
      completedTodos: {type: Array},
      isGeneratingGmailTodos_: {type: Boolean},
      hasGmailGenerationError_: {type: Boolean},
      hasGeneratedGmail_: {type: Boolean},
      isCompletedExpanded_: {type: Boolean},
      // Tab-based todo properties.
      tabTodos: {type: Array},
      completedTabTodos: {type: Array},
      isGeneratingTabTodos_: {type: Boolean},
      hasTabGenerationError_: {type: Boolean},
      hasGeneratedTab_: {type: Boolean},
      isCompletedTabExpanded_: {type: Boolean},
      // Reading list properties.
      readingListTodos: {type: Array},
    };
  }

  accessor todos: AutoTodoItem[]|null = null;
  accessor completedTodos: AutoTodoItem[]|null = null;
  accessor tabTodos: AutoTodoItem[]|null = null;
  accessor completedTabTodos: AutoTodoItem[]|null = null;
  accessor readingListTodos: AutoTodoItem[]|null = null;
  protected accessor autoTodosEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTodos');
  protected accessor showingReadingList_: boolean = false;

  // Gmail-based property accessors.
  protected accessor isGeneratingGmailTodos_: boolean = false;
  protected accessor hasGmailGenerationError_: boolean = false;
  protected accessor hasGeneratedGmail_: boolean = false;
  protected accessor isCompletedExpanded_: boolean = false;

  // Tab-based property accessors.
  protected accessor isGeneratingTabTodos_: boolean = false;
  // TODO(crbug.com/539697847): Use this to show an error message to the user.
  protected accessor hasTabGenerationError_: boolean = false;
  protected accessor hasGeneratedTab_: boolean = false;
  protected accessor isCompletedTabExpanded_: boolean = false;

  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    if (this.autoTodosEnabled_) {
      this.listenerIds_.push(
          browserProxyFactory.getInstance()
              .callbackRouter.onAutoTodosChanged.addListener(
                  (todos: AutoTodoItem[]) => {
                    this.todos =
                        todos
                            .filter(
                                todo => !!todo.data.firstParty &&
                                    todo.status === AutoTodoStatus.kActive)
                            .sort((a, b) => b.score - a.score);
                    this.completedTodos =
                        todos
                            .filter(
                                todo => !!todo.data.firstParty &&
                                    todo.status === AutoTodoStatus.kCompleted)
                            .sort((a, b) => b.score - a.score);
                    this.tabTodos =
                        todos
                            .filter(
                                todo => !!todo.data.thirdParty &&
                                    todo.data.thirdParty.groupType !==
                                        AutoTodoGroup.kReadingList &&
                                    todo.status === AutoTodoStatus.kActive)
                            .sort(
                                (a, b) => getTabTodoPriority(a) -
                                    getTabTodoPriority(b));
                    this.completedTabTodos =
                        todos
                            .filter(
                                todo => !!todo.data.thirdParty &&
                                    todo.data.thirdParty.groupType !==
                                        AutoTodoGroup.kReadingList &&
                                    todo.status === AutoTodoStatus.kCompleted)
                            .sort(
                                (a, b) => getTabTodoPriority(a) -
                                    getTabTodoPriority(b));
                    this.readingListTodos =
                        todos
                            .filter(
                                todo => !!todo.data.thirdParty &&
                                    todo.data.thirdParty.groupType ===
                                        AutoTodoGroup.kReadingList &&
                                    todo.status !== AutoTodoStatus.kDismissed)
                            .sort(
                                (a, b) => getTabTodoPriority(a) -
                                    getTabTodoPriority(b));
                  }));
      this.listenerIds_.push(
          browserProxyFactory.getInstance()
              .callbackRouter.onFirstPartyAutoTodosGenerationStateChanged
              .addListener((isGenerating: boolean) => {
                this.isGeneratingGmailTodos_ = isGenerating;
              }));
      this.listenerIds_.push(
          browserProxyFactory.getInstance()
              .callbackRouter.onThirdPartyAutoTodosGenerationStateChanged
              .addListener((isGenerating: boolean) => {
                this.isGeneratingTabTodos_ = isGenerating;
              }));
      this.fetchAutoTodos_();
    }
  }

  private async fetchAutoTodos_() {
    try {
      const {firstPartyTodos, thirdPartyTodos} =
          await browserProxyFactory.getInstance().handler.getAutoTodos();
      this.todos =
          firstPartyTodos.filter(todo => todo.status === AutoTodoStatus.kActive)
              .sort((a, b) => b.score - a.score) ??
          null;
      this.completedTodos =
          firstPartyTodos
              .filter(todo => todo.status === AutoTodoStatus.kCompleted)
              .sort((a, b) => b.score - a.score) ??
          null;
      this.tabTodos =
          thirdPartyTodos
              .filter(
                  todo => todo.data.thirdParty?.groupType !==
                          AutoTodoGroup.kReadingList &&
                      todo.status === AutoTodoStatus.kActive)
              .sort((a, b) => getTabTodoPriority(a) - getTabTodoPriority(b)) ??
          null;
      this.completedTabTodos =
          thirdPartyTodos
              .filter(
                  todo => todo.data.thirdParty?.groupType !==
                          AutoTodoGroup.kReadingList &&
                      todo.status === AutoTodoStatus.kCompleted)
              .sort((a, b) => getTabTodoPriority(a) - getTabTodoPriority(b)) ??
          null;
      this.readingListTodos =
          thirdPartyTodos
              .filter(
                  todo => todo.data.thirdParty?.groupType ===
                          AutoTodoGroup.kReadingList &&
                      todo.status !== AutoTodoStatus.kDismissed)
              .sort((a, b) => getTabTodoPriority(a) - getTabTodoPriority(b)) ??
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

  protected onGoToReadingListClick_() {
    this.showingReadingList_ = true;
  }

  protected onBackClick_() {
    this.showingReadingList_ = false;
  }

  protected onCompletedExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.isCompletedExpanded_ = e.detail.value;
  }

  protected onCompletedTabExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.isCompletedTabExpanded_ = e.detail.value;
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
