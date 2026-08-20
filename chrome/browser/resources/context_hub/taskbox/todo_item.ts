// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {AutoTodoGroup, AutoTodoStatus, browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {AutoTodoData, AutoTodoItem, SourceReference} from '../context_hub.mojom-webui.js';

import {getCss} from './todo_item.css.js';
import {getHtml} from './todo_item.html.js';

const FORM_URL =
    'https://docs.google.com/forms/d/e/1FAIpQLSdKC1R7AWz6L0rRlCsCkF9cT7Q4KqGCU8mfT2qNFyWscUVo8g/viewform';
const ENTRY_LIKED = 'entry.1262687224';
const ENTRY_TITLE = 'entry.809272442';
const ENTRY_DESCRIPTION = 'entry.1908093752';
const ENTRY_SCORE = 'entry.1904072234';

export enum TodoItemVariant {
  DEFAULT = 'default',
  TAB = 'tab',
}

export interface TodoItemElement {
  $: {
    menu?: CrActionMenuElement,
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
      lastActiveTimestamp: {type: Object},
      groupType: {type: Number},
      expanded_: {type: Boolean},
      liked: {type: Boolean},
      variant: {type: String},
      disable_state_mgmt: {type: Boolean},
    };
  }

  override accessor id: string = '';
  accessor heading: string = '';
  accessor description: string = '';
  accessor status: AutoTodoStatus = AutoTodoStatus.kActive;
  accessor actionableUrl: string = '';
  accessor sourceReferences: SourceReference[] = [];
  accessor score: number|null = null;
  tabId: bigint|null = null;
  accessor lastActiveTimestamp: Time|null = null;
  accessor groupType: AutoTodoGroup = AutoTodoGroup.kNoMatch;
  protected accessor expanded_: boolean = false;
  accessor liked: boolean|null = null;
  accessor variant: TodoItemVariant = TodoItemVariant.DEFAULT;
  accessor disable_state_mgmt: boolean = false;

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
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

  protected async onThumbClick_(e: Event, like: boolean) {
    e.stopPropagation();
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

    // Prefill the form with the todo item details.
    const params = new URLSearchParams({
      'usp': 'pp_url',
      [ENTRY_LIKED]: like ? 'Liked' : 'Disliked',
      [ENTRY_TITLE]: this.heading,
      [ENTRY_DESCRIPTION]: this.description,
    });
    if (this.score !== null && this.score !== undefined) {
      params.set(ENTRY_SCORE, this.score.toFixed(2));
    }
    window.open(`${FORM_URL}?${params.toString()}`, '_blank');
  }

  private async updateStatus_(status: AutoTodoStatus) {
    if (this.disable_state_mgmt) {
      return;
    }

    if (this.variant === TodoItemVariant.TAB && this.tabId === null) {
      return;
    }

    const data: AutoTodoData = this.variant === TodoItemVariant.TAB ?
        {
          thirdParty: {
            tabId: this.tabId!,
            lastActiveTimestamp:
                this.lastActiveTimestamp ?? {internalValue: 0n},
            groupType: this.groupType,
          },
        } :
        {
          firstParty: {
            actionableUrl: this.actionableUrl,
            sourceReferences: this.sourceReferences,
          },
        };

    const todo: AutoTodoItem = {
      id: this.id,
      title: this.heading,
      description: this.description,
      status,
      score: this.score ?? 0,
      data,
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

  protected async onDismissClick_(e: Event) {
    e.stopPropagation();
    this.$.menu?.close();
    await this.updateStatus_(AutoTodoStatus.kDismissed);
  }

  protected onMoreClick_(e: Event) {
    e.stopPropagation();
    this.$.menu?.showAt(e.currentTarget as HTMLElement);
  }

  protected onOpenTabClick_(e: Event) {
    e.stopPropagation();
    if (this.variant === TodoItemVariant.TAB) {
      if (this.tabId !== null) {
        browserProxyFactory.getInstance().handler.switchToTab(this.tabId);
      }
      return;
    }

    if (this.actionableUrl) {
      window.open(this.actionableUrl, '_blank');
    }
  }

  protected onCloseTabClick_(e: Event) {
    e.stopPropagation();
    this.$.menu?.close();
    if (this.tabId !== null) {
      browserProxyFactory.getInstance().handler.closeTab(this.tabId);
    }
  }

  private async addToReadingList_() {
    if (this.disable_state_mgmt || this.tabId === null) {
      return;
    }

    const data: AutoTodoData = {
      thirdParty: {
        tabId: this.tabId,
        lastActiveTimestamp: this.lastActiveTimestamp ?? {internalValue: 0n},
        groupType: AutoTodoGroup.kReadingList,
      },
    };

    const todo: AutoTodoItem = {
      id: this.id,
      title: this.heading,
      description: this.description,
      status: this.status,
      score: 0,
      data,
    };
    try {
      await browserProxyFactory.getInstance().handler.updateAutoTodo(todo);
    } catch (e) {
      console.error('Failed to add auto todo to reading list:', e);
    }
  }

  protected async onSaveClick_(e: Event) {
    e.stopPropagation();
    this.$.menu?.close();
    await this.addToReadingList_();
  }

  protected getReferences() {
    if (!this.sourceReferences) {
      return [];
    }
    return this.sourceReferences
        .map(ref => {
          if (ref.gmail) {
            return {
              label: ref.gmail.subject.trim() || 'Gmail',
              url: ref.gmail.messageUrl,
            };
          }
          return null;
        })
        .filter((ref): ref is {label: string, url: string} => ref !== null);
  }

  protected onActionsClick_(e: Event) {
    // Prevent clicking actions from toggling the expand button.
    e.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'todo-item': TodoItemElement;
  }
}

customElements.define(TodoItemElement.is, TodoItemElement);
