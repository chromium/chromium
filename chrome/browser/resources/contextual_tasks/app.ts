// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './top_toolbar.js';
import '//resources/cr_components/composebox/composebox.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';

export class ContextualTasksAppElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-app';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      threadUrl_: {type: String},
    };
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  protected accessor threadUrl_: string = '';

  // TODO(crbug.com/454388385): Remove this once the authentication flow is
  // implemented. Removing the gsc param renders the OGB header, which allows
  // the user to press "Sign In" to authenticate.
  protected removeGsc_() {
    const url = new URL(this.threadUrl_);
    url.searchParams.delete('gsc');
    this.threadUrl_ = url.toString();
  }

  override async connectedCallback() {
    super.connectedCallback();

    // Check if the URL that loaded this page has a task attached to it. If it
    // does, we'll use the tasks URL to load the embedded page.
    const params = new URLSearchParams(window.location.search);
    const taskUuid = params.get('task');
    if (taskUuid) {
      const {url} = await this.browserProxy_.getUrlForTask({value: taskUuid});
      this.browserProxy_.setTaskId({value: taskUuid});

      const aiPageParams = new URLSearchParams(new URL(url.url).search);
      this.browserProxy_.setThreadTitle(aiPageParams.get('q') || '');
      this.threadUrl_ = url.url;
    } else {
      const {url} = await this.browserProxy_.getThreadUrl();
      this.threadUrl_ = url.url;
    }

    // Tell the browser the WebUI is loaded and ready to show in side panel. If
    // the WebUI is loadded in a tab it's an no-op.
    this.browserProxy_.showUi();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
