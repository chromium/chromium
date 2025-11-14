// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_textarea/cr_textarea.js';
import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {Tab} from '../contextual_tasks_internals.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

export class ContextualTasksInternalsAppElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      relevantTabs_: {type: Array},
      query_: {type: String},
      isQueryPending_: {type: Boolean},
    };
  }

  protected accessor relevantTabs_: Tab[] = [];
  protected accessor query_: string = '';
  protected accessor isQueryPending_: boolean = false;

  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  protected onQueryChanged_(e: CustomEvent<{value: string}>) {
    this.query_ = e.detail.value;
  }

  protected async onSubmitClick_() {
    this.isQueryPending_ = true;
    const response =
        await this.proxy_.handler.getRelevantContext({query: this.query_});
    this.relevantTabs_ = response.response.relevantTabs;
    this.isQueryPending_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-internals-app': ContextualTasksInternalsAppElement;
  }
}

customElements.define(
    ContextualTasksInternalsAppElement.is, ContextualTasksInternalsAppElement);
