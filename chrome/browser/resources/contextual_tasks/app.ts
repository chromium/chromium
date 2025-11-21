// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox.js';
import './top_toolbar.js';

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {Tab, Thread} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';

type ChromeEventFunctionType<T> =
    T extends ChromeEvent<infer ListenerType>? ListenerType : never;

export interface ContextualTasksAppElement {
  $: {
    threadFrame: chrome.webviewTag.WebView,
  };
}

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
  private oauthToken_: string = '';

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

  protected onOpenInNewTabClick_() {
    this.browserProxy_.handler.moveTaskUiToToNewTab();
  }

  protected onOpenChromeSettingsClick_() {
    this.browserProxy_.handler.openChromeSettingsUi();
  }

  protected onMyActivityClick_() {
    this.browserProxy_.handler.openMyActivityUi();
  }

  protected onHelpClick_() {
    this.browserProxy_.handler.openHelpUi();
  }

  protected onTabClick_(e: CustomEvent<Tab>) {
    this.browserProxy_.handler.onTabClickedFromSourcesMenu(
        e.detail.tabId, e.detail.url);
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

    // Setup the webview request overrides before loading the first URL.
    // TODO(crbug.com/461596412): Fetching the OAuth token is async, so there
    // is no guarantee by the time the URL below is loaded, the OAuth is
    // present. Ideally, the OAuth will always be ready early, but if not, hold
    // the initial request until the OAuth is ready.
    this.setupWebviewRequestOverrides();

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
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.$.threadFrame.request.onBeforeSendHeaders.removeListener(
        this.onBeforeSendHeaders.bind(this));
  }

  override render() {
    return getHtml.bind(this)();
  }

  private async updateToolbarVisibility() {
    const {isInTab} = await this.browserProxy_.handler.isShownInTab();
    this.isShownInTab_ = isInTab;
  }

  private setupWebviewRequestOverrides() {
    // TODO(crbug.com/461595196): Currently, this grabs the OAuth token once,
    // but it should be refreshed if it expires.
    this.browserProxy_.handler.getOAuthToken().then(({oauthToken}) => {
      this.oauthToken_ = oauthToken;
    });

    // Setup the webview request overrides to add the OAuth token to the request
    // headers.
    this.$.threadFrame.request.onBeforeSendHeaders.addListener(
        this.onBeforeSendHeaders.bind(this), {
          // These should be valid values from web_request.d.ts.
          types: 'main_frame,xmlhttprequest,websocket'.split(',') as any,
          urls: ['<all_urls>'],
        },
        ['blocking', 'requestHeaders', 'extraHeaders']);

    // TODO(crbug.com/454388385): Remove "WGA/1.0" once our custom user agent
    // is allowlisted. This is a temporary workaround to unblock the
    // authentication flow.
    const userAgent = this.$.threadFrame.getUserAgent();
    this.$.threadFrame.setUserAgentOverride(userAgent + ' WGA/1.0');
  }

  private onBeforeSendHeaders:
      ChromeEventFunctionType<typeof chrome.webRequest.onBeforeSendHeaders> =
          (details): chrome.webRequest.BlockingResponse => {
            // Return a promise that will be resolved with the new request
            // headers. This will block the request until the OAuth token is
            // fetched.
            const requestHeaders = details.requestHeaders || [];
            requestHeaders.push({
              'name': 'Authorization',
              'value': `Bearer ${this.oauthToken_}`,
            });
            return {requestHeaders};
          };
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
