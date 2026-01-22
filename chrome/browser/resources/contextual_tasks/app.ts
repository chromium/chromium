// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox.js';
import './error_page.js';
import './top_toolbar.js';

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {ContextualTasksComposeboxElement} from './composebox.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {PostMessageHandler} from './post_message_handler.js';

type ChromeEventFunctionType<T> =
    T extends ChromeEvent<infer ListenerType>? ListenerType : never;

// The url query parameter keys for the viewport size.
const VIEWPORT_HEIGHT_KEY = 'bih';
const VIEWPORT_WIDTH_KEY = 'biw';

export interface ContextualTasksAppElement {
  $: {
    threadFrame: chrome.webviewTag.WebView,
    composebox: ContextualTasksComposeboxElement,
    composeboxHeaderWrapper: HTMLElement,
    composeboxHeader: HTMLElement,
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
      darkMode_: {
        type: Boolean,
        reflect: true,
      },
      isErrorPageVisible_: {
        type: Boolean,
        reflect: true,
      },
      isInBasicMode_: {type: Boolean, reflect: true},
      // Means no queries have been submitted in current AIM thread.
      isZeroState_: {
        type: Boolean,
        reflect: true,
      },
      isAiPage_: {type: Boolean, reflect: true},
      isLensOverlayShowing_: {type: Boolean},
    };
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  protected accessor isAiPage_: boolean = true;
  protected accessor isLensOverlayShowing_: boolean = false;
  // Indicates if in tab mode. Most start in a tab.
  protected accessor isShownInTab_: boolean = true;
  protected accessor darkMode_: boolean = loadTimeData.getBoolean('darkMode');
  private pendingUrl_: string = '';
  protected accessor threadTitle_: string = '';
  protected accessor isInBasicMode_: boolean = false;
  protected accessor isErrorPageVisible_: boolean = false;
  protected accessor isZeroState_: boolean = false;

  protected friendlyZeroStateSubtitle: string =
      loadTimeData.getString('friendlyZeroStateSubtitle');
  protected friendlyZeroStateTitle: string =
      loadTimeData.getString('friendlyZeroStateTitle');
  private listenerIds_: number[] = [];
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
    this.$.threadFrame.src = url;
    this.$.composebox.startExpandAnimation();
    this.$.composebox.clearInputAndFocus();
  }

  override async connectedCallback() {
    super.connectedCallback();

    // Record the WebUI URL in case one of the events below fires and changes
    // it.
    const webUiUrlOnLoad = new URL(window.location.href);

    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [
      callbackRouter.onSidePanelStateChanged.addListener(
          () => this.updateSidePanelState()),
      callbackRouter.setThreadTitle.addListener((title: string) => {
        this.threadTitle_ = title;
        updateTitleInUrl(title);
        document.title = title || loadTimeData.getString('title');
      }),
      callbackRouter.onAiPageStatusChanged.addListener((isAiPage: boolean) => {
        this.isAiPage_ = isAiPage;
      }),
      callbackRouter.postMessageToWebview.addListener(
          this.postMessageToWebview.bind(this)),
      callbackRouter.onHandshakeComplete.addListener(
          this.onHandshakeComplete.bind(this)),

      // TODO(crbug.com/474359572): Rename this to be more descriptive of what
      // it actually does.
      callbackRouter.hideInput.addListener(() => {
        this.isInBasicMode_ = true;
      }),
      callbackRouter.restoreInput.addListener(() => {
        this.isInBasicMode_ = false;
      }),
      callbackRouter.setTaskDetails.addListener(updateTaskDetailsInUrl),
      callbackRouter.onZeroStateChange.addListener((isZeroState: boolean) => {
        this.isZeroState_ = isZeroState;
      }),
      callbackRouter.onLensOverlayStateChanged.addListener(
          (isOverlayShowing: boolean) => {
            this.isLensOverlayShowing_ = isOverlayShowing;
          }),
      callbackRouter.showErrorPage.addListener(() => {
        this.isErrorPageVisible_ = true;
      }),
      callbackRouter.hideErrorPage.addListener(() => {
        this.isErrorPageVisible_ = false;
      }),
    ];

    this.updateSidePanelState();

    // Fetch the initial common search params.
    this.updateCommonSearchParams();

    // Setup the webview request overrides before loading the first URL.
    this.setupWebviewRequestOverrides();

    // Check if the URL that loaded this page has a task attached to it. If it
    // does, we'll use the tasks URL to load the embedded page.
    const taskUuid = webUiUrlOnLoad.searchParams.get('task');
    let threadUrl = '';
    if (taskUuid) {
      const {url} =
          await this.browserProxy_.handler.getUrlForTask({value: taskUuid});
      this.browserProxy_.handler.setTaskId({value: taskUuid});

      const aiPageParams = new URLSearchParams(new URL(url).search);
      this.browserProxy_.handler.setThreadTitle(aiPageParams.get('q') || '');
      threadUrl = url;
    } else {
      const {url} = await this.browserProxy_.handler.getThreadUrl();
      threadUrl = url;
      this.$.composebox.clearInputAndFocus();
    }

    const threadUrlAsUrl = new URL(threadUrl);

    // If the "open_history" param has any value, open the history panel.
    if (webUiUrlOnLoad.searchParams.has('open_history')) {
      threadUrlAsUrl.searchParams.set('atvm', '1');

      // Remove the param so subsequent loads don't show history again.
      const url = new URL(window.location.href);
      url.searchParams.delete('open_history');
      window.history.replaceState({}, '', url.href);
    }

    // Check if the initial render should be zero state.
    const {isZeroState} =
        await this.browserProxy_.handler.isZeroState(threadUrlAsUrl.href);
    this.isZeroState_ = isZeroState;

    // The thread URL is considered pending (not loaded immediately in the
    // webview) until oauth tokens are received from the WebUI controller. This
    // prevents situations where the user is technically signed out of the
    // embedded frame and unable to save or access existing data.
    this.pendingUrl_ =
        this.maybeUpdateThreadUrlForRestore(threadUrlAsUrl, webUiUrlOnLoad);
    this.maybeLoadPendingUrl_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
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
  private maybeUpdateThreadUrlForRestore(threadUrl: URL, webUiUrl: URL):
      string {
    // Check if the provided URL is default by checking for thread ID, turn ID,
    // and title. If those params are not present, but are present on the WebUI
    // URL, apply them to the thread URL.
    // TODO(470107169): The ContextualTasksService should provide this URL
    //                  based on task ID alone.
    const updatedThreadUrl = new URL(threadUrl.href);
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
    // pending URL.
    if (this.pendingUrl_ && this.commonSearchParams_) {
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

  getThreadUrlForTesting() {
    return this.$.threadFrame.src;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
