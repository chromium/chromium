// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './todo_item.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/icons.html.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {AutoTodoStatus, browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {AutoTodoItem} from '../context_hub.mojom-webui.js';

import {getCss} from './ai_taskbox.css.js';
import {getHtml} from './ai_taskbox.html.js';

const GENERAL_FEEDBACK_FORM_URL = 'https://forms.gle/sfEC2J7QBuz6zmbD7';

function convertMojoTimeToDate(mojoTime: Time): Date {
  const unixEpochUs = mojoTime.internalValue - 11644473600000000n;
  return new Date(Number(unixEpochUs / 1000n));
}

function compareTodos(a: AutoTodoItem, b: AutoTodoItem): number {
  return b.score - a.score || a.title.localeCompare(b.title);
}

function filterTodos(
    todos: AutoTodoItem[], status: AutoTodoStatus): AutoTodoItem[] {
  return todos.filter(todo => !!todo.data.firstParty && todo.status === status)
      .sort(compareTodos);
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
      feedbackModeEnabled_: {type: Boolean},
      todos: {type: Array},
      completedTodos: {type: Array},
      dismissedTodos: {type: Array},
      isGeneratingGmailTodos_: {type: Boolean},
      hasGmailGenerationError_: {type: Boolean},
      hasGeneratedGmail_: {type: Boolean},
      isCompletedExpanded_: {type: Boolean},
      isDismissedExpanded_: {type: Boolean},
      lastGmailGenerationTime_: {type: Object},
      feedbacks_: {type: Object},
    };
  }

  accessor todos: AutoTodoItem[]|null = null;
  accessor completedTodos: AutoTodoItem[]|null = null;
  accessor dismissedTodos: AutoTodoItem[]|null = null;
  protected accessor feedbacks_: Map<string, boolean> = new Map();
  protected accessor autoTodosEnabled_: boolean =
      loadTimeData.getBoolean('kAutoTodos');
  protected accessor feedbackModeEnabled_: boolean = true;

  protected accessor isGeneratingGmailTodos_: boolean = false;
  protected accessor hasGmailGenerationError_: boolean = false;
  protected accessor hasGeneratedGmail_: boolean = false;
  protected accessor isCompletedExpanded_: boolean = false;
  protected accessor isDismissedExpanded_: boolean = false;
  protected accessor lastGmailGenerationTime_: Date|null = null;

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
                    this.todos = filterTodos(todos, AutoTodoStatus.kActive);
                    this.completedTodos =
                        filterTodos(todos, AutoTodoStatus.kCompleted);
                    this.dismissedTodos =
                        filterTodos(todos, AutoTodoStatus.kDismissed);
                  }));
      this.listenerIds_.push(
          browserProxyFactory.getInstance()
              .callbackRouter.onFirstPartyAutoTodosGenerationStateChanged
              .addListener((isGenerating: boolean) => {
                this.isGeneratingGmailTodos_ = isGenerating;
                if (isGenerating) {
                  this.hasGmailGenerationError_ = false;
                } else if (!this.hasGmailGenerationError_) {
                  this.lastGmailGenerationTime_ = new Date();
                  this.hasGeneratedGmail_ = true;
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
          firstPartyMetadata,
        },
        {feedbacks},
      ] =
          await Promise.all([
            browserProxyFactory.getInstance().handler.getAutoTodos(),
            browserProxyFactory.getInstance().handler.getTodoFeedbacks(),
          ]);
      this.hasGmailGenerationError_ = firstPartyMetadata.hasError;
      this.lastGmailGenerationTime_ =
          (firstPartyMetadata.lastGenerationTime &&
           firstPartyMetadata.lastGenerationTime.internalValue > 0n) ?
          convertMojoTimeToDate(firstPartyMetadata.lastGenerationTime) :
          null;
      this.hasGeneratedGmail_ = this.lastGmailGenerationTime_ !== null;
      const feedbackMap = new Map<string, boolean>();
      for (const feedback of feedbacks) {
        feedbackMap.set(feedback.todoId, feedback.liked);
      }
      this.feedbacks_ = feedbackMap;

      this.todos = filterTodos(firstPartyTodos, AutoTodoStatus.kActive);
      this.completedTodos =
          filterTodos(firstPartyTodos, AutoTodoStatus.kCompleted);
      this.dismissedTodos =
          filterTodos(firstPartyTodos, AutoTodoStatus.kDismissed);
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

  protected onFeedbackModeToggleChange_(e: CustomEvent<boolean>) {
    this.feedbackModeEnabled_ = e.detail;
  }

  protected onGeneralFeedbackClick_() {
    window.open(GENERAL_FEEDBACK_FORM_URL, '_blank');
  }

  protected onCompletedExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.isCompletedExpanded_ = e.detail.value;
  }

  protected onDismissedExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.isDismissedExpanded_ = e.detail.value;
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
      console.error('Failed to generate auto todos:', e);
      this.hasGmailGenerationError_ = true;
    } finally {
      this.isGeneratingGmailTodos_ = false;
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

  protected onWorkspaceMenuClick_(e: Event) {
    e.stopPropagation();
    this.shadowRoot?.querySelector<CrActionMenuElement>('#workspaceMenu')
        ?.showAt(e.currentTarget as HTMLElement);
  }

  protected async onClearWorkspaceTodosClick_() {
    if (!this.autoTodosEnabled_ || this.isGeneratingGmailTodos_) {
      return;
    }
    this.shadowRoot?.querySelector<CrActionMenuElement>('#workspaceMenu')
        ?.close();
    try {
      const {success} = await browserProxyFactory.getInstance()
                            .handler.clearFirstPartyAutoTodos();
      if (success) {
        this.todos = null;
        this.completedTodos = null;
        this.dismissedTodos = null;
        this.hasGeneratedGmail_ = false;
        this.lastGmailGenerationTime_ = null;
        this.hasGmailGenerationError_ = false;
      }
    } catch (e) {
      console.error('Failed to clear todos:', e);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-taskbox': AiTaskboxElement;
  }
}

customElements.define(AiTaskboxElement.is, AiTaskboxElement);
