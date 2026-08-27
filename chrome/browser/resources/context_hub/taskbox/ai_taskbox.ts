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
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {AutoTodoGroup, AutoTodoStatus, browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {AutoTodoItem} from '../context_hub.mojom-webui.js';

import {getCss} from './ai_taskbox.css.js';
import {getHtml} from './ai_taskbox.html.js';

const GENERAL_FEEDBACK_FORM_URL = 'https://forms.gle/sfEC2J7QBuz6zmbD7';

function convertMojoTimeToDate(mojoTime: Time): Date {
  const unixEpochUs = mojoTime.internalValue - 11644473600000000n;
  return new Date(Number(unixEpochUs / 1000n));
}

function getTabTodoPriority(item: AutoTodoItem): number {
  const group = item.data.thirdParty?.groupType;

  // Pending action todos are the highest priority and should be shown first.
  if (group === AutoTodoGroup.kUnfinishedAction) {
    return 0;
  }
  if (group === AutoTodoGroup.kShoppingCart) {
    return 1;
  }
  if (group === AutoTodoGroup.kNudgeToClose) {
    return 2;
  }
  return 3;
}

function compareFirstPartyTodos(a: AutoTodoItem, b: AutoTodoItem): number {
  return b.score - a.score || a.title.localeCompare(b.title);
}

function compareThirdPartyTodos(a: AutoTodoItem, b: AutoTodoItem): number {
  return getTabTodoPriority(a) - getTabTodoPriority(b) ||
      a.title.localeCompare(b.title);
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
      lastGmailGenerationTime_: {type: Object},
      // Tab-based todo properties.
      tabTodos: {type: Array},
      completedTabTodos: {type: Array},
      isGeneratingTabTodos_: {type: Boolean},
      hasTabGenerationError_: {type: Boolean},
      hasGeneratedTab_: {type: Boolean},
      isCompletedTabExpanded_: {type: Boolean},
      lastTabGenerationTime_: {type: Object},
      // Reading list properties.
      readingListTodos: {type: Array},
      feedbacks_: {type: Object},
    };
  }

  accessor todos: AutoTodoItem[]|null = null;
  accessor completedTodos: AutoTodoItem[]|null = null;
  accessor tabTodos: AutoTodoItem[]|null = null;
  accessor completedTabTodos: AutoTodoItem[]|null = null;
  accessor readingListTodos: AutoTodoItem[]|null = null;
  protected accessor feedbacks_: Map<string, boolean> = new Map();
  protected accessor autoTodosEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTodos');
  protected accessor showingReadingList_: boolean = false;

  // Gmail-based property accessors.
  protected accessor isGeneratingGmailTodos_: boolean = false;
  protected accessor hasGmailGenerationError_: boolean = false;
  protected accessor hasGeneratedGmail_: boolean = false;
  protected accessor isCompletedExpanded_: boolean = false;
  protected accessor lastGmailGenerationTime_: Date|null = null;

  // Tab-based property accessors.
  protected accessor isGeneratingTabTodos_: boolean = false;
  protected accessor hasTabGenerationError_: boolean = false;
  protected accessor hasGeneratedTab_: boolean = false;
  protected accessor isCompletedTabExpanded_: boolean = false;
  protected accessor lastTabGenerationTime_: Date|null = null;

  private listenerIds_: number[] = [];
  private updateTimerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    if (this.autoTodosEnabled_) {
      // Update the "Last updated" status every minute.
      this.updateTimerId_ =
          window.setInterval(() => this.requestUpdate(), 60000);
      this.listenerIds_.push(
          browserProxyFactory.getInstance()
              .callbackRouter.onAutoTodosChanged.addListener(
                  (todos: AutoTodoItem[]) => {
                    this.todos =
                        todos
                            .filter(
                                todo => !!todo.data.firstParty &&
                                    todo.status === AutoTodoStatus.kActive)
                            .sort(compareFirstPartyTodos);
                    this.completedTodos =
                        todos
                            .filter(
                                todo => !!todo.data.firstParty &&
                                    todo.status === AutoTodoStatus.kCompleted)
                            .sort(compareFirstPartyTodos);
                    this.tabTodos =
                        todos
                            .filter(
                                todo => !!todo.data.thirdParty &&
                                    todo.data.thirdParty.groupType !==
                                        AutoTodoGroup.kReadingList &&
                                    todo.status === AutoTodoStatus.kActive)
                            .sort(compareThirdPartyTodos);
                    this.completedTabTodos =
                        todos
                            .filter(
                                todo => !!todo.data.thirdParty &&
                                    todo.data.thirdParty.groupType !==
                                        AutoTodoGroup.kReadingList &&
                                    todo.status === AutoTodoStatus.kCompleted)
                            .sort(compareThirdPartyTodos);
                    this.readingListTodos =
                        todos
                            .filter(
                                todo => !!todo.data.thirdParty &&
                                    todo.data.thirdParty.groupType ===
                                        AutoTodoGroup.kReadingList &&
                                    todo.status !== AutoTodoStatus.kDismissed)
                            .sort(compareThirdPartyTodos);
                  }));
      this.listenerIds_.push(
          browserProxyFactory.getInstance()
              .callbackRouter.onFirstPartyAutoTodosGenerationStateChanged
              .addListener((isGenerating: boolean) => {
                this.isGeneratingGmailTodos_ = isGenerating;
                if (!isGenerating) {
                  this.lastGmailGenerationTime_ = new Date();
                }
              }));
      this.listenerIds_.push(
          browserProxyFactory.getInstance()
              .callbackRouter.onThirdPartyAutoTodosGenerationStateChanged
              .addListener((isGenerating: boolean) => {
                this.isGeneratingTabTodos_ = isGenerating;
                if (!isGenerating) {
                  this.lastTabGenerationTime_ = new Date();
                }
              }));
      this.fetchAutoTodos_();
    }
  }

  private async fetchAutoTodos_() {
    try {
      const [
        {
          firstPartyTodos,
          thirdPartyTodos,
          lastFirstPartyGenerationTime,
          lastThirdPartyGenerationTime,
        },
        {feedbacks},
      ] =
          await Promise.all([
            browserProxyFactory.getInstance().handler.getAutoTodos(),
            browserProxyFactory.getInstance().handler.getTodoFeedbacks(),
          ]);
      if (lastFirstPartyGenerationTime &&
          lastFirstPartyGenerationTime.internalValue > 0n) {
        this.lastGmailGenerationTime_ =
            convertMojoTimeToDate(lastFirstPartyGenerationTime);
      }
      if (lastThirdPartyGenerationTime &&
          lastThirdPartyGenerationTime.internalValue > 0n) {
        this.lastTabGenerationTime_ =
            convertMojoTimeToDate(lastThirdPartyGenerationTime);
      }
      const feedbackMap = new Map<string, boolean>();
      for (const feedback of feedbacks) {
        feedbackMap.set(feedback.todoId, feedback.liked);
      }
      this.feedbacks_ = feedbackMap;

      this.todos =
          firstPartyTodos.filter(todo => todo.status === AutoTodoStatus.kActive)
              .sort(compareFirstPartyTodos) ??
          null;
      this.completedTodos =
          firstPartyTodos
              .filter(todo => todo.status === AutoTodoStatus.kCompleted)
              .sort(compareFirstPartyTodos) ??
          null;
      this.tabTodos = thirdPartyTodos
                          .filter(
                              todo => todo.data.thirdParty?.groupType !==
                                      AutoTodoGroup.kReadingList &&
                                  todo.status === AutoTodoStatus.kActive)
                          .sort(compareThirdPartyTodos) ??
          null;
      this.completedTabTodos =
          thirdPartyTodos
              .filter(
                  todo => todo.data.thirdParty?.groupType !==
                          AutoTodoGroup.kReadingList &&
                      todo.status === AutoTodoStatus.kCompleted)
              .sort(compareThirdPartyTodos) ??
          null;
      this.readingListTodos =
          thirdPartyTodos
              .filter(
                  todo => todo.data.thirdParty?.groupType ===
                          AutoTodoGroup.kReadingList &&
                      todo.status !== AutoTodoStatus.kDismissed)
              .sort(compareThirdPartyTodos) ??
          null;
    } catch (e) {
      console.error('Failed to fetch auto todos:', e);
    }
  }

  protected onFeedbackChanged_(
      e: CustomEvent<{todoId: string, liked: boolean|null}>) {
    const {todoId, liked} = e.detail;
    const newFeedbackMap = new Map(this.feedbacks_);
    if (liked === null) {
      newFeedbackMap.delete(todoId);
    } else {
      newFeedbackMap.set(todoId, liked);
    }
    this.feedbacks_ = newFeedbackMap;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.updateTimerId_ !== null) {
      window.clearInterval(this.updateTimerId_);
      this.updateTimerId_ = null;
    }
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

  protected getUnfinishedTabTodos_(): AutoTodoItem[] {
    return this.tabTodos?.filter(
               todo => todo.data.thirdParty?.groupType ===
                   AutoTodoGroup.kUnfinishedAction) ??
        [];
  }

  protected getShoppingCartTabTodos_(): AutoTodoItem[] {
    return this.tabTodos?.filter(
               todo => todo.data.thirdParty?.groupType ===
                   AutoTodoGroup.kShoppingCart) ??
        [];
  }

  protected getStaleTabTodos_(): AutoTodoItem[] {
    return this.tabTodos?.filter(
               todo => todo.data.thirdParty?.groupType ===
                   AutoTodoGroup.kNudgeToClose) ??
        [];
  }

  protected onCloseAllStaleTabsClick_() {
    for (const todo of this.getStaleTabTodos_()) {
      const tabId = todo.data.thirdParty?.tabId;
      if (tabId !== null && tabId !== undefined) {
        browserProxyFactory.getInstance().handler.closeTab(tabId);
      }
    }
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
        this.lastGmailGenerationTime_ = new Date();
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
        this.lastTabGenerationTime_ = new Date();
      }
    } catch (e) {
      console.error('Failed to generate tab-based todos:', e);
      this.hasTabGenerationError_ = true;
    } finally {
      this.isGeneratingTabTodos_ = false;
    }
  }

  protected getFormattedTimeAgo_(date: Date|null): string {
    if (!date) {
      return 'Click the refresh icon to generate';
    }
    const now = Date.now();
    const diffMs = Math.max(0, now - date.getTime());
    const diffSec = Math.floor(diffMs / 1000);
    const diffMin = Math.floor(diffSec / 60);
    const diffHours = Math.floor(diffMin / 60);
    const diffDays = Math.floor(diffHours / 24);

    if (diffMin < 1) {
      return 'Last updated just now';
    }
    if (diffHours < 1) {
      return diffMin === 1 ? 'Last updated 1 minute ago' :
                             `Last updated ${diffMin} minutes ago`;
    }
    if (diffDays < 1) {
      return diffHours === 1 ? 'Last updated 1 hour ago' :
                               `Last updated ${diffHours} hours ago`;
    }
    return diffDays === 1 ? 'Last updated 1 day ago' :
                            `Last updated ${diffDays} days ago`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-taskbox': AiTaskboxElement;
  }
}

customElements.define(AiTaskboxElement.is, AiTaskboxElement);
