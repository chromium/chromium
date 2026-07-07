// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {SourceReference} from '../context_hub.mojom-webui.js';

import {getCss} from './todo_item.css.js';
import {getHtml} from './todo_item.html.js';

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
      heading: {type: String},
      description: {type: String},
      actionableUrl: {type: String},
      sourceReferences: {type: Array},
      expanded_: {type: Boolean},
    };
  }

  accessor heading: string = '';
  accessor description: string = '';
  accessor actionableUrl: string = '';
  accessor sourceReferences: SourceReference[] = [];
  protected accessor expanded_: boolean = false;

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  protected onStartClick_(e: Event) {
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

  protected onMenuClick_(e: MouseEvent) {
    e.stopPropagation();
    this.$.menu.showAt(e.currentTarget as HTMLElement);
  }

  protected onFeedbackClick_() {
    this.$.menu.close();
    window.open('https://forms.gle/sfEC2J7QBuz6zmbD7', '_blank');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'todo-item': TodoItemElement;
  }
}

customElements.define(TodoItemElement.is, TodoItemElement);
