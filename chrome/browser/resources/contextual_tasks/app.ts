// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox.js';
import './error_page.js';
import './top_toolbar.js';
import './zero_state_overlay.js';

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {ContextualTasksComposeboxElement} from './composebox.js';
import type {Tab} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {PostMessageHandler} from './post_message_handler.js';
import type {ZeroStateOverlayElement} from './zero_state_overlay.js';

type ChromeEventFunctionType<T> =
    T extends ChromeEvent<infer ListenerType>? ListenerType : never;

// The url query parameter keys for the viewport size.
const VIEWPORT_HEIGHT_KEY = 'bih';
const VIEWPORT_WIDTH_KEY = 'biw';

export interface ContextualTasksAppElement {
  $: {
    threadFrame: chrome.webviewTag.WebView,
    composebox: ContextualTasksComposeboxElement,
    zeroStateOverlay: ZeroStateOverlayElement,
  };
}

// Updates the params for task ID, thread ID, and turn ID in the URL without
// reloading the page or adding to history.
function updateTaskDetailsInUrl(
    taskId: Uuid, threadId: string, turnId: string) {
  const url = new URL(window.location.href);

  url.searchParams.set('task', taskId.value);

  threadId ? url.searchParams.set('thread', threadId) :
             url.searchParams.delete('thread');

  turnId ? url.searchParams.set('turn', turnId) :
           url.searchParams.delete('turn');

  window.history.replaceState({}, '', url.href);
}

// Updates param for the title in the WebUI URL. This facilitates the restore
// flow on refresh or restart.
function updateTitleInUrl(title: string) {
  const url = new URL(window.location.href);

  url.searchParams.set('title', title);

  window.history.replaceState({}, '', url.href);
}

// Returns whether the URL that is used in the embedded thread frame has
// appropriate params to load an existing thread, as opposed to the default
// zero-state.
function embeddedUrlHasThreadParams(url: URL): boolean {
  return url.searchParams.has('mstk') && url.searchParams.has('mtid') &&
      url.searchParams.has('q');
}

// Returns whether the WebUI URL (the outer frame) has params that facilitate
// loading an existing thread as opposed to the default zero state.
function webUiUrlHasThreadParams(url: URL): boolean {
  return url.searchParams.has('thread') && url.searchParams.has('turn') &&
      url.searchParams.has('title');
}

function applyWebUiParamsToThreadUrl(threadUrl: URL, webUiUrl: URL) {
  threadUrl.searchParams.set('mtid', webUiUrl.searchParams.get('thread') || '');
  threadUrl.searchParams.set('mstk', webUiUrl.searchParams.get('turn') || '');
  // This value doesn't actually influence the result provided by AI mode
  // if thread ID and turn ID are provided, but is required to display
  // anything other than the zero-state.
  threadUrl.searchParams.set('q', webUiUrl.searchParams.get('title') || '');
}

export class ContextualTasksAppElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isShownInTab_: {
        type: Boolean,
        reflect: true,
      },
      threadTitle_: {type: String},
      contextTabs_: {type: Array},
      darkMode_: {
        type: Boolean,
        reflect: true,
      },
      isErrorPageVisible_: {
        type: Boolean,
        reflect: true,
      },
      showComposebox_: {
        type: Boolean,
        reflect: true,
      },
      // Means no queries have been submitted in current AIM thread.
      isZeroState_: {
        type: Boolean,
        reflect: true,
      },
      isAiPage_: {type: Boolean, reflect: true},
    };
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  protected accessor isAiPage_: boolean = false;
  // Indicates if in tab mode. Most start in a tab.
  protected accessor isShownInTab_: boolean = true;
  protected accessor darkMode_: boolean = loadTimeData.getBoolean('darkMode');
  private pendingUrl_: string = '';
  protected accessor threadTitle_: string = '';
  protected accessor contextTabs_: Tab[] = [];
  protected accessor showComposebox_: boolean = true;
  protected accessor isErrorPageVisible_: boolean = false;
  protected accessor isZeroState_: boolean = false;
  private listenerIds_: number[] = [];
  // The OAuth token to use for embedded page requests. Null if not yet set.
  // Can be empty if the user is not signed in or the token couldn't be fetched.
  private oauthToken_: string|null = null;
  private commonSearchParams_: {[key: string]: string} = {};
  private postMessageHandler_!: PostMessageHandler;
  private forcedEmbeddedPageHost =
      loadTimeData.getString('forcedEmbeddedPageHost');
  private signInDomains_: string[] =
      loadTimeData.getString('contextualTasksSignInDomains').split(',');

  override firstUpdated() {
    this.postMessageHandler_ =
        new PostMessageHandler(this.$.threadFrame, this.browserProxy_);
  }

  protected async onNewThreadClick_() {
    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.OpenNewThread');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.OpenNewThread', true);
    const {url} = await this.browserProxy_.handler.getThreadUrl();
    this.$.threadFrame.src = url.url;
    this.$.composebox.startExpandAnimation();
    this.$.zeroStateOverlay.startOverlayAnimation();
    this.$.composebox.clearInputAndFocus();
  }

  override async connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [
      callbackRouter.onSidePanelStateChanged.addListener(
          () => this.updateSidePanelState()),
      callbackRouter.setThreadTitle.addListener((title: string) => {
        this.threadTitle_ = title;
        updateTitleInUrl(title);
        document.title = title || loadTimeData.getString('title');
      }),
      callbackRouter.postMessageToWebview.addListener(
          this.postMessageToWebview.bind(this)),
      callbackRouter.onHandshakeComplete.addListener(
          this.onHandshakeComplete.bind(this)),
      callbackRouter.onContextUpdated.addListener((tabs: Tab[]) => {
        this.contextTabs_ = tabs;
      }),
      callbackRouter.setOAuthToken.addListener((oauthToken: string) => {
        this.oauthToken_ = oauthToken;
        this.maybeLoadPendingUrl_();
      }),
      callbackRouter.hideInput.addListener(() => {
        this.showComposebox_ = false;
      }),
      callbackRouter.restoreInput.addListener(() => {
        this.showComposebox_ = true;
      }),
      callbackRouter.setTaskDetails.addListener(updateTaskDetailsInUrl),
      callbackRouter.onZeroStateChange.addListener((isZeroState: boolean) => {
        this.isZeroState_ = isZeroState;
      }),
    ];

    this.updateSidePanelState();

    // Fetch the initial common search params.
    this.updateCommonSearchParams();

    // Setup the webview request overrides before loading the first URL.
    this.setupWebviewRequestOverrides();

    // Check if the URL that loaded this page has a task attached to it. If it
    // does, we'll use the tasks URL to load the embedded page.
    const params = new URLSearchParams(window.location.search);
    const taskUuid = params.get('task');
    let threadUrl = '';
    if (taskUuid) {
      const {url} =
          await this.browserProxy_.handler.getUrlForTask({value: taskUuid});
      this.browserProxy_.handler.setTaskId({value: taskUuid});

      const aiPageParams = new URLSearchParams(new URL(url.url).search);
      this.browserProxy_.handler.setThreadTitle(aiPageParams.get('q') || '');
      threadUrl = url.url;
    } else {
      const {url} = await this.browserProxy_.handler.getThreadUrl();
      threadUrl = url.url;
      this.$.composebox.clearInputAndFocus();
    }

    // Check if the initial render should be zero state.
    const threadUrlAsUrl = new URL(threadUrl);
    const {isZeroState} = await this.browserProxy_.handler.isZeroState(
        {url: threadUrlAsUrl.href} as Url);
    this.isZeroState_ = isZeroState;

    // The thread URL is considered pending (not loaded immediately in the
    // webview) until oauth tokens are received from the WebUI controller. This
    // prevents situations where the user is technically signed out of the
    // embedded frame and unable to save or access existing data.
    this.pendingUrl_ = this.maybeUpdateThreadUrlForRestore(threadUrl);
    this.maybeLoadPendingUrl_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.$.threadFrame.request.onBeforeSendHeaders.removeListener(
        this.onBeforeSendHeaders);
    this.$.threadFrame.request.onBeforeRequest.removeListener(
        this.onBeforeRequest);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    // Fetch the common search params before setting up the request overrides.
    // TODO(crbug.com/463729504): Add checking to see if dark mode changed.
    if (changedPrivateProperties.has('isShownInTab_')) {
      this.updateCommonSearchParams();
    }
  }

  // Conditionally update the provided thread URL so it restores an existing
  // thread. If the thread URL already contains the params for loading a
  // specific thread, this will return the same URL that was provided.
  private maybeUpdateThreadUrlForRestore(threadUrl: string): string {
    // Check if the provided URL is default by checking for thread ID, turn ID,
    // and title. If those params are not present, but are present on the WebUI
    // URL, apply them to the thread URL.
    // TODO(470107169): The ContextualTasksService should provide this URL
    //                  based on task ID alone.
    const updatedThreadUrl = new URL(threadUrl);
    const webUiUrl = new URL(window.location.href);
    const threadUrlHasParams = embeddedUrlHasThreadParams(updatedThreadUrl);
    const webUiUrlHasParams = webUiUrlHasThreadParams(webUiUrl);
    if (!threadUrlHasParams && webUiUrlHasParams) {
      applyWebUiParamsToThreadUrl(updatedThreadUrl, webUiUrl);
      this.threadTitle_ =
          webUiUrl.searchParams.get('title') || loadTimeData.getString('title');
      document.title = this.threadTitle_;
    }

    return updatedThreadUrl.href;
  }

  private postMessageToWebview(message: number[]) {
    this.postMessageHandler_.sendMessage(new Uint8Array(message));
  }

  private maybeLoadPendingUrl_() {
    // If all the data needed to make the initial request is available, load the
    // pending URL. If the OAuth token is empty, that signifies that the user is
    // not signed in, so the URL can still be loaded. If the OAuth token is
    // null, and therefore not yet set, do not load the URL.
    if (this.pendingUrl_ && this.commonSearchParams_ &&
        this.oauthToken_ != null) {
      this.$.threadFrame.src = this.pendingUrl_;
      this.pendingUrl_ = '';
    }
  }

  private onHandshakeComplete() {
    this.postMessageHandler_.completeHandshake();
  }

  private async updateSidePanelState() {
    const {isInTab} = await this.browserProxy_.handler.isShownInTab();
    this.isShownInTab_ = isInTab;
  }

  private async updateCommonSearchParams() {
    // TODO(crbug.com/463729504): Add support for dark mode.
    const {params} = await this.browserProxy_.handler.getCommonSearchParams(
        /*isDarkMode=*/ this.darkMode_,
        /*isSidePanel=*/ !this.isShownInTab_);
    this.commonSearchParams_ = params;
    this.maybeLoadPendingUrl_();
  }

  private setupWebviewRequestOverrides() {
    // Setup the webview request overrides to add the OAuth token to the request
    // headers.
    this.$.threadFrame.request.onBeforeSendHeaders.addListener(
        this.onBeforeSendHeaders, {
          // These should be valid values from web_request.d.ts.
          types: 'main_frame,xmlhttprequest,websocket'.split(',') as any,
          urls: ['<all_urls>'],
        },
        ['blocking', 'requestHeaders', 'extraHeaders']);

    this.$.threadFrame.request.onBeforeRequest.addListener(
        this.onBeforeRequest, {
          types: ['main_frame'] as any,
          urls: ['<all_urls>'],
        },
        ['blocking']);

    // Sets the user agent to the default user agent + the contextual tasks
    // custom suffix.
    const userAgent = this.$.threadFrame.getUserAgent();
    const userAgentSuffix = loadTimeData.getString('userAgentSuffix');
    this.$.threadFrame.setUserAgentOverride(`${userAgent} ${userAgentSuffix}`);
  }

  private addCommonSearchParams(url: URL): URL {
    for (const [key, value] of Object.entries(this.commonSearchParams_)) {
      if (value === '') {
        url.searchParams.delete(key);
      } else {
        url.searchParams.set(key, value);
      }
    }

    // The viewport width and height are also common search params but they
    // are not retrieved from the browser as the viewport is determined by the
    // contextual tasks webview.
    const resultsBoundingRect = this.$.threadFrame.getBoundingClientRect();
    if (resultsBoundingRect.width > 0) {
      url.searchParams.set(
          VIEWPORT_WIDTH_KEY, resultsBoundingRect.width.toString());
    }
    if (resultsBoundingRect.height > 0) {
      url.searchParams.set(
          VIEWPORT_HEIGHT_KEY, resultsBoundingRect.height.toString());
    }

    return url;
  }

  private onBeforeRequest:
      ChromeEventFunctionType<typeof chrome.webRequest.onBeforeRequest> =
          (details): chrome.webRequest.BlockingResponse => {
            const url = new URL(details.url);
            const newUrl = this.addCommonSearchParams(url);
            const isSigninDomain =
                !!this.signInDomains_.find((domain) => domain === url.host);
            if (this.forcedEmbeddedPageHost && !isSigninDomain) {
              newUrl.host = this.forcedEmbeddedPageHost;
            }
            if (newUrl.href !== details.url) {
              return {redirectUrl: newUrl.href};
            }
            return {};
          };

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

  getPendingUrlForTesting() {
    return this.pendingUrl_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
