// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {SourceReference} from '../context_hub.mojom-webui.js';

import {getCss} from './todo_item.css.js';
import {getHtml} from './todo_item.html.js';

const FORM_URL =
    'https://docs.google.com/forms/d/e/1FAIpQLSdKC1R7AWz6L0rRlCsCkF9cT7Q4KqGCU8mfT2qNFyWscUVo8g/viewform';
const ENTRY_LIKED = 'entry.1262687224';
const ENTRY_TITLE = 'entry.809272442';
const ENTRY_DESCRIPTION = 'entry.1908093752';
const ENTRY_SCORE = 'entry.1904072234';

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
      actionableUrl: {type: String},
      sourceReferences: {type: Array},
      score: {type: Number},
      expanded_: {type: Boolean},
      liked: {type: Boolean},
    };
  }

  override accessor id: string = '';
  accessor heading: string = '';
  accessor description: string = '';
  accessor actionableUrl: string = '';
  accessor sourceReferences: SourceReference[] = [];
  accessor score: number = 0;
  protected accessor expanded_: boolean = false;
  accessor liked: boolean|null = null;

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  protected getThumbsUpIcon_(): string {
    return this.liked === true ? 'cr:thumbs-up-filled' : 'cr:thumbs-up';
  }

  protected getThumbsDownIcon_(): string {
    return this.liked === false ? 'cr:thumbs-down-filled' : 'cr:thumbs-down';
  }

  protected onThumbsUpClick_(e: Event) {
    this.onThumbClick_(e, true);
  }

  protected onThumbsDownClick_(e: Event) {
    this.onThumbClick_(e, false);
  }

  protected onThumbClick_(e: Event, like: boolean) {
    e.stopPropagation();
    // If the thumb is already clicked, unselect it.
    if (this.liked === like) {
      this.liked = null;
      return;
    }

    this.liked = like;
    // Prefill the form with the todo item details.
    const params = new URLSearchParams({
      'usp': 'pp_url',
      [ENTRY_LIKED]: like ? 'Liked' : 'Disliked',
      [ENTRY_TITLE]: this.heading,
      [ENTRY_DESCRIPTION]: this.description,
      [ENTRY_SCORE]: this.score.toFixed(2),
    });
    window.open(`${FORM_URL}?${params.toString()}`, '_blank');
  }

  protected onCheckCircleClick_(e: Event) {
    e.stopPropagation();
    // TODO(crbug.com/541016246): Implement check circle click.
  }

  protected onDismissClick_(e: Event) {
    e.stopPropagation();
    // TODO(crbug.com/541016246): Implement dismiss click.
  }

  protected onOpenTabClick_(e: Event) {
    e.stopPropagation();
    if (this.actionableUrl) {
      window.open(this.actionableUrl, '_blank');
    }
  }

  protected getReferences() {
    if (!this.sourceReferences) {
      return [];
    }
    return this.sourceReferences
        .map(ref => {
          if (ref.gmail) {
            return {
              label: 'Gmail',
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
