// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './todo_item.css.js';
import {getHtml} from './todo_item.html.js';

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
      expanded_: {type: Boolean},
    };
  }

  accessor heading: string = '';
  accessor description: string = '';
  protected accessor expanded_: boolean = false;

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
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
