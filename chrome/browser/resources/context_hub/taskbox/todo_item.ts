// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '../icons.html.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {AutoTodoStatus, browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {AutoTodoItem, SourceReference} from '../context_hub.mojom-webui.js';

import {getCss} from './todo_item.css.js';
import {getHtml} from './todo_item.html.js';

const FIRST_PARTY_FORM_URL =
    'https://docs.google.com/forms/d/e/1FAIpQLSdKC1R7AWz6L0rRlCsCkF9cT7Q4KqGCU8mfT2qNFyWscUVo8g/viewform';
const ENTRY_LIKED = 'entry.1262687224';
const ENTRY_DETAILS = 'entry.809272442';
const MAX_URLS_TO_OPEN = 10;

// Only web pages are openable, matching the browser-side filter in
// `PageHandler::OpenUrlsInTabGroup()`.
function isWebUrl(url: string): boolean {
  if (!url) {
    return false;
  }
  try {
    const {protocol} = new URL(url);
    return protocol === 'https:' || protocol === 'http:';
  } catch {
    return false;
  }
}

interface SourceReferenceItem {
  label: string;
  url: string;
}

export interface TodoItemElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export class TodoItemElement extends CrLitElement {
  static get is() {
    return 'todo-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      id: {type: String},
      heading: {type: String},
      description: {type: String},
      status: {type: Number},
      actionableUrl: {type: String},
      sourceReferences: {type: Array},
      score: {type: Number},
      liked: {type: Boolean},
      feedbackMode: {type: Boolean},
      disable_state_mgmt: {type: Boolean},
      disable_feedback: {type: Boolean},
      isRunningTask_: {type: Boolean},
      references_: {type: Array},
    };
  }

  override accessor id: string = '';
  accessor heading: string = '';
  accessor description: string = '';
  accessor status: AutoTodoStatus = AutoTodoStatus.kActive;
  accessor actionableUrl: string = '';
  accessor sourceReferences: SourceReference[] = [];
  accessor score: number|null|undefined = null;
  accessor liked: boolean|null = null;
  accessor feedbackMode: boolean = false;
  accessor disable_state_mgmt: boolean = false;
  // TODO(b/586300169): Support thumbs up/down feedback for Launchpad tasks.
  accessor disable_feedback: boolean = false;
  // TODO(b/586300169): Allow `canActOnWeb_` with `kLaunchpad` once
  // `RunTodoTask` mojom `RuntimeFeature` gating is updated.
  protected readonly canActOnWeb_: boolean =
      loadTimeData.valueExists('kAutoTodos') &&
      loadTimeData.getBoolean('kAutoTodos') &&
      loadTimeData.valueExists('can_act_on_web') &&
      loadTimeData.getBoolean('can_act_on_web');
  protected accessor isRunningTask_: boolean = false;
  protected accessor references_: SourceReferenceItem[] = [];

  private openableReferenceUrls_: string[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('sourceReferences')) {
      this.references_ = this.computeReferences_();
      this.openableReferenceUrls_ = this.references_.map(item => item.url);
    }
  }

  protected getCheckCircleIcon_(): string {
    return this.status === AutoTodoStatus.kCompleted ?
        'cr:check-circle' :
        'context-hub:radio-button-unchecked';
  }

  protected getThumbsUpIcon_(): string {
    return this.liked === true ? 'cr:thumb-up-filled' : 'cr:thumb-up';
  }

  protected getThumbsDownIcon_(): string {
    return this.liked === false ? 'cr:thumb-down-filled' : 'cr:thumb-down';
  }

  protected async onThumbsUpClick_(e: Event) {
    await this.onThumbClick_(e, true);
  }

  protected async onThumbsDownClick_(e: Event) {
    await this.onThumbClick_(e, false);
  }

  private async onThumbClick_(e: Event, like: boolean) {
    e.stopPropagation();
    if (this.disable_feedback) {
      return;
    }
    // If the thumb is already clicked, unselect it.
    if (this.liked === like) {
      this.liked = null;
      try {
        await browserProxyFactory.getInstance().handler.deleteTodoFeedback(
            this.id);
      } catch (e) {
        console.error('Failed to delete todo feedback:', e);
      }
      this.fire('feedback-changed', {todoId: this.id, liked: null});
      return;
    }

    this.liked = like;
    try {
      await browserProxyFactory.getInstance().handler.setTodoFeedback({
        todoId: this.id,
        liked: like,
      });
    } catch (e) {
      console.error('Failed to set todo feedback:', e);
    }
    this.fire('feedback-changed', {todoId: this.id, liked: like});

    const scoreStr = this.score !== null && this.score !== undefined ?
        this.score.toFixed(2) :
        '';
    const details = [
      this.heading,
      this.description,
      this.actionableUrl,
      scoreStr,
    ].join(' / ');

    const params = new URLSearchParams({
      'usp': 'pp_url',
      [ENTRY_LIKED]: like ? 'Liked' : 'Disliked',
      [ENTRY_DETAILS]: details,
    });
    window.open(`${FIRST_PARTY_FORM_URL}?${params.toString()}`, '_blank');
  }

  private async updateStatus_(status: AutoTodoStatus) {
    if (this.disable_state_mgmt) {
      return;
    }

    const todo: AutoTodoItem = {
      id: this.id,
      title: this.heading,
      description: this.description,
      status,
      score: this.score ?? 0,
      data: {
        firstParty: {
          actionableUrl: this.actionableUrl || null,
          sourceReferences: this.sourceReferences,
        },
      },
    };
    try {
      await browserProxyFactory.getInstance().handler.updateAutoTodo(todo);
    } catch (e) {
      console.error('Failed to update auto todo status:', e);
    }
  }

  protected async onCheckCircleClick_(e: Event) {
    e.stopPropagation();
    const nextStatus = this.status === AutoTodoStatus.kCompleted ?
        AutoTodoStatus.kActive :
        AutoTodoStatus.kCompleted;
    await this.updateStatus_(nextStatus);
  }

  protected async onDismissButtonClick_(e: Event) {
    e.stopPropagation();
    this.$.menu.close();
    await this.updateStatus_(AutoTodoStatus.kDismissed);
  }

  protected async onMenuMarkActiveButtonClick_(e: Event) {
    e.stopPropagation();
    this.$.menu.close();
    await this.updateStatus_(AutoTodoStatus.kActive);
  }

  protected onMoreButtonClick_(e: Event) {
    e.stopPropagation();
    this.$.menu.showAt(e.currentTarget as HTMLElement);
  }

  protected async onOpenTabClick_(e: Event) {
    e.stopPropagation();
    if (this.actionableUrl) {
      window.open(this.actionableUrl, '_blank');
      return;
    }
    await this.openReferencesInTabGroup_();
  }

  protected async onRunTaskButtonClick_(e: Event) {
    e.stopPropagation();
    const taskSummary = this.getTaskSummary_();
    if (this.disable_state_mgmt || this.isRunningTask_ || !taskSummary) {
      return;
    }
    const prompt = `${taskSummary} If you need more information ` +
        `to complete your task or if you have any questions, ask.`;

    this.isRunningTask_ = true;
    try {
      await browserProxyFactory.getInstance().handler.runTodoTask(prompt);
    } catch (err) {
      console.error('Failed to run todo task:', err);
    } finally {
      this.isRunningTask_ = false;
    }
  }

  private async openReferencesInTabGroup_() {
    const urls = this.openableReferenceUrls_;
    if (urls.length === 0) {
      return;
    }

    const groupLabel = this.heading || 'Related Tabs';
    try {
      const {success} =
          await browserProxyFactory.getInstance().handler.openUrlsInTabGroup(
              groupLabel, urls);
      if (success) {
        return;
      }
    } catch (err) {
      console.error('Failed to open tabs in group:', err);
    }

    // Fallback to opening individual tabs.
    for (const url of urls.slice(0, MAX_URLS_TO_OPEN)) {
      OpenWindowProxyImpl.getInstance().openUrl(url);
    }
  }

  private computeReferences_(): SourceReferenceItem[] {
    if (!this.sourceReferences) {
      return [];
    }
    const references: SourceReferenceItem[] = [];
    for (const item of this.sourceReferences) {
      let label = '';
      let url = '';
      if (item.gmail) {
        url = item.gmail.messageUrl;
        label = item.gmail.subject.trim() || 'Gmail';
      } else if (item.chromeHistoryEntry) {
        url = item.chromeHistoryEntry.url;
        label = item.chromeHistoryEntry.title.trim() || url;
      } else if (item.searchQuery) {
        // The browser only surfaces search references it could build a
        // search URL for (Google is the default search provider). A blank
        // query has nothing to show, so leave `url` empty to drop it below.
        label = item.searchQuery.query.trim();
        url = label ? item.searchQuery.searchUrl : '';
      }
      if (isWebUrl(url)) {
        references.push({label, url});
      }
    }
    return references;
  }

  private getTaskSummary_(): string {
    const targetUrl =
        this.actionableUrl || this.openableReferenceUrls_[0] || '';
    return [this.heading.trim(), targetUrl].filter(Boolean).join(' ');
  }

  protected canRunTask_(): boolean {
    return Boolean(this.getTaskSummary_());
  }

  protected canOpenTab_(): boolean {
    return Boolean(this.actionableUrl) ||
        this.openableReferenceUrls_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'todo-item': TodoItemElement;
  }
}

customElements.define(TodoItemElement.is, TodoItemElement);
