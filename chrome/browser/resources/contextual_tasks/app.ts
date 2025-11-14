// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './top_toolbar.js';
import '//resources/cr_components/composebox/composebox.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {Thread} from './contextual_tasks.mojom-webui.js';
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
      isShownInTab_: {type: Boolean},
      threadUrl_: {type: String},
      threadTitle_: {type: String},
      historyThreads_: {type: Array},
    };
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  protected accessor isShownInTab_: boolean = true;  // Most start in a tab.
  protected accessor threadUrl_: string = '';
  protected accessor threadTitle_: string = '';
  protected accessor historyThreads_: Thread[] = [];
  private listenerIds_: number[] = [];

  // TODO(crbug.com/454388385): Remove this once the authentication flow is
  // implemented. Removing the gsc param renders the OGB header, which allows
  // the user to press "Sign In" to authenticate.
  protected removeGsc_() {
    const url = new URL(this.threadUrl_);
    url.searchParams.delete('gsc');
    this.threadUrl_ = url.toString();
  }

  protected onCloseButtonClick_() {
    this.browserProxy_.handler.closeSidePanel();
  }

  protected async onNewThreadClick_() {
    const {url} = await this.browserProxy_.handler.getThreadUrl();
    this.threadUrl_ = url.url;
  }

  protected async onThreadHistoryClick_() {
    const {threads} = await this.browserProxy_.handler.showThreadHistory();
    this.historyThreads_ = threads;
    // TODO(crbug.com/445469925): Display the threads in a drawer.
  }

  protected onOpenInNewTabClick_() {}

  protected onOpenChromeSettingsClick_() {
    this.browserProxy_.handler.openChromeSettingsUi();
  }

  protected onMyActivityClick_() {
    this.browserProxy_.handler.openMyActivityUi();
  }

  protected onHelpClick_() {
    this.browserProxy_.handler.openHelpUi();
  }

  override async connectedCallback() {
    super.connectedCallback();

    this.listenerIds_.push(
        this.browserProxy_.callbackRouter.onSidePanelStateChanged.addListener(
            () => this.updateToolbarVisibility()),
        this.browserProxy_.callbackRouter.setThreadTitle.addListener(
            (title: string) => {
              this.threadTitle_ = title;
            }));

    this.updateToolbarVisibility();

    // Check if the URL that loaded this page has a task attached to it. If it
    // does, we'll use the tasks URL to load the embedded page.
    const params = new URLSearchParams(window.location.search);
    const taskUuid = params.get('task');
    if (taskUuid) {
      const {url} =
          await this.browserProxy_.handler.getUrlForTask({value: taskUuid});
      this.browserProxy_.handler.setTaskId({value: taskUuid});

      const aiPageParams = new URLSearchParams(new URL(url.url).search);
      this.browserProxy_.handler.setThreadTitle(aiPageParams.get('q') || '');
      this.threadUrl_ = url.url;
    } else {
      const {url} = await this.browserProxy_.handler.getThreadUrl();
      this.threadUrl_ = url.url;
    }
  }

  override disconnectedCallback() {
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
  }

  override render() {
    return getHtml.bind(this)();
  }

  private async updateToolbarVisibility() {
    const {isInTab} = await this.browserProxy_.handler.isShownInTab();
    this.isShownInTab_ = isInTab;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
