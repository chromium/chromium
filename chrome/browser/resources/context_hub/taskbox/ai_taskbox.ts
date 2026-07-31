// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './todo_item.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

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
    };
  }

  accessor todos: AutoTodoItem[]|null = null;

  protected onGeneralFeedbackClick_() {
    window.open(GENERAL_FEEDBACK_FORM_URL, '_blank');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ai-taskbox': AiTaskboxElement;
  }
}

customElements.define(AiTaskboxElement.is, AiTaskboxElement);
