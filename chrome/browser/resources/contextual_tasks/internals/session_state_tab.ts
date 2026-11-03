// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../contextual_tasks_internals.mojom-webui.js';
import type {BrowserProxy, WebContentsDebugState} from '../contextual_tasks_internals.mojom-webui.js';

import {getCss} from './session_state_tab.css.js';
import {getHtml} from './session_state_tab.html.js';

export class ContextualTasksSessionStateTabElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-session-state-tab';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      webContentsDebugStates_: {type: Array},
    };
  }

  protected accessor webContentsDebugStates_: WebContentsDebugState[] = [];

  private proxy_: BrowserProxy = browserProxyFactory.getInstance();

  async refresh() {
    const response = await this.proxy_.handler.getSessionAndUserDataState();
    this.webContentsDebugStates_ = response.webContentsStates;
  }

  protected async onRefreshSessionStateClick_() {
    await this.refresh();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-session-state-tab': ContextualTasksSessionStateTabElement;
  }
}

customElements.define(
    ContextualTasksSessionStateTabElement.is,
    ContextualTasksSessionStateTabElement);
