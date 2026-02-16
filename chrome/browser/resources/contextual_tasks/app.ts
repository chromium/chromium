// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './composebox.js';
import './error_dialog.js';
import './error_page.js';
import './ghost_loader.js';
import './top_toolbar.js';

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {ContextualTasksComposeboxElement} from './composebox.js';
import type {ComposeboxPosition} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {PostMessageHandler} from './post_message_handler.js';

declare global {
  interface HTMLElementEventMap {
    'newwindow': chrome.webviewTag.NewWindowEvent;
  }
}

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
    flexCenterContainer: HTMLElement,
  };
}

// Updates the params for task ID, thread ID, and turn ID in the URL without
// reloading the page or adding to history.
function updateTaskDetailsInUrl(
    taskId: Uuid, threadId: string, turnId: string) {
  const url = new URL(window.location.href);

  const existingTaskId = url.searchParams.get('task');
  url.searchParams.set('task', taskId.value);

  threadId ? url.searchParams.set('thread', threadId) :
             url.searchParams.delete('thread');

  turnId ? url.searchParams.set('turn', turnId) :
           url.searchParams.delete('turn');

  // Allow back navigation if the task ID changes. Other changes to the URL
  // represent state changes for the current task.
  if (existingTaskId !== taskId.value) {
    window.history.pushState({}, '', url.href);
  } else {
    window.history.replaceState({}, '', url.href);
  }
}

// Preserve the aim url to the search param named aim_url used for reload and
// session restore.
function updateAimUrl(aimUrl: any) {
  const url = new URL(window.location.href);
  url.searchParams.set('aim_url', aimUrl);
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
      isGhostLoaderVisible_: {type: Boolean, reflect: true},
      isErrorDialogVisible_: {type: Boolean},
      enableNativeZeroStateSuggestions: {
        type: Boolean,
        reflect: true,
      },
      enableBasicModeZOrder_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor enableNativeZeroStateSuggestions: boolean =
      loadTimeData.getBoolean('enableNativeZeroStateSuggestions');
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  protected accessor enableBasicModeZOrder_: boolean =
      loadTimeData.getBoolean('enableBasicModeZOrder');
  protected accessor isAiPage_: boolean = true;
  protected accessor isLensOverlayShowing_: boolean = false;
  // Indicates if in tab mode. Most start in a tab.
  protected accessor isShownInTab_: boolean = true;
  protected accessor darkMode_: boolean = loadTimeData.getBoolean('darkMode');
  protected accessor isErrorDialogVisible_: boolean = false;
  private pendingUrl_: string = '';
  protected accessor threadTitle_: string = '';
  protected accessor isInBasicMode_: boolean = false;
  protected accessor isErrorPageVisible_: boolean = false;
  protected accessor isZeroState_: boolean = false;

  protected friendlyZeroStateSubtitle: string =
      loadTimeData.getString('friendlyZeroStateSubtitle');
  protected friendlyZeroStateTitle: string =
      loadTimeData.getString('friendlyZeroStateTitle');
  protected accessor isGhostLoaderVisible_: boolean = false;
  // Tracks whether the frame is currently loading. Needed to avoid race
  // condition while awaiting isAiPage.
  private isFrameLoading: boolean = false;
  private listenerIds_: number[] = [];
  private eventTracker_: EventTracker = new EventTracker();
  private commonSearchParams_: {[key: string]: string} = {};
  private postMessageHandler_!: PostMessageHandler;
  private forcedEmbeddedPageHost =
      loadTimeData.getString('forcedEmbeddedPageHost');
  private signInDomains_: string[] =
      loadTimeData.getString('contextualTasksSignInDomains').split(',');
  private enableGhostLoader_: boolean =
      loadTimeData.getBoolean('enableGhostLoader');
  // A callback to allow tests to wait until the popstate handler in this class
  // has finished running.
  private popStateFinishedCallbackForTesting_: (() => void)|null = null;
  private forceBasicModeIfOpeningThreadHistory_: boolean =
      loadTimeData.getBoolean('forceBasicModeIfOpeningThreadHistory');

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
    const newThreadUrl = new URL(url);
    const currentUrl = new URL(this.$.threadFrame.src);
    const source = currentUrl.searchParams.get('source');
    if (source) {
      newThreadUrl.searchParams.set('source', source);
    }
    const aep = currentUrl.searchParams.get('aep');
    if (aep) {
      newThreadUrl.searchParams.set('aep', aep);
    }
    this.$.threadFrame.src = newThreadUrl.href;
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
      callbackRouter.setAimUrl.addListener(updateAimUrl),
      callbackRouter.onZeroStateChange.addListener((isZeroState: boolean) => {
        this.isZeroState_ = isZeroState;
        // If we just changed to zero state, that means
        // it is a new thread or new AIM page. Otherwise,
        // we are not in zero state anymore, or not in an AIM URL. In
        // both thread/AIM cases for zero state, we clear input.
        if (isZeroState) {
          this.$.composebox.clearInputAndFocus();
        }
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
      callbackRouter.showOauthErrorDialog.addListener(() => {
        this.isErrorDialogVisible_ = true;
      }),
      callbackRouter.updateComposeboxPosition.addListener(
          this.onUpdateComposeboxPosition_.bind(this),
          ),
    ];

    this.eventTracker_.add(window, 'popstate', async () => {
      // The back button may pop state that was pushed by a task change. If that
      // is the case, fetch the URL for the task ID and load that in the frame.
      const taskUuid = new URLSearchParams(location.search).get('task');
      if (taskUuid) {
        const {url} =
            await this.browserProxy_.handler.getUrlForTask({value: taskUuid});

        // Do nothing if the app element is no longer attached to the page. This
        // can occur in tests where awaiting the call above will delay the rest
        // of this handler and affect other tests in the suite.
        if (!this.isConnected) {
          return;
        }

        this.browserProxy_.handler.setTaskId({value: taskUuid});
        this.$.threadFrame.src = url;

        // Allow tests to wait for this callback to complete.
        if (this.popStateFinishedCallbackForTesting_) {
          this.popStateFinishedCallbackForTesting_();
        }
      }
    });

    this.updateSidePanelState();

    // Fetch the initial common search params.
    this.updateCommonSearchParams();

    // Listeners for ghost loader
    if (this.enableGhostLoader_) {
      this.$.threadFrame.addEventListener('contentload', () => {
        this.isFrameLoading = false;
        this.setIsGhostLoaderVisible(false);
      });
      this.$.threadFrame.addEventListener('loadabort', () => {
        this.isFrameLoading = false;
        this.setIsGhostLoaderVisible(false);
      });
      this.$.threadFrame.addEventListener('loadstart', async (ev: any) => {
        if (!ev.isTopLevel) {
          return;
        }
        this.isFrameLoading = true;
        const { isAiPage } =
          await this.browserProxy_.handler.isAiPage(ev.url as string);
        if (this.isFrameLoading && !isAiPage) {
          this.setIsGhostLoaderVisible(true);
        }
      });
    }

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
    // If the thread URL has parameters to open history, set basic mode.
    if (this.hasThreadHistoryParams(threadUrlAsUrl) &&
        this.forceBasicModeIfOpeningThreadHistory_) {
      this.isInBasicMode_ = true;
    }

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
    this.eventTracker_.removeAll();
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
  private setStyleVariable(variable: string, value: string) {
    this.$.composebox.style.setProperty(variable, `${value}px`);
  }
  /* Adjust composebox based on server notifications. Negatives are used if
   * server wants to change marginTop, marginRight.
   */
  private onUpdateComposeboxPosition_(position: ComposeboxPosition) {
    if (position.maxWidth !== null) {
      this.setStyleVariable('--max-composebox-width', `${position.maxWidth}px`);
    }
    if (position.maxHeight !== null) {
      // Set contextual task's composebox max-height.
      this.setStyleVariable(
          '--max-composebox-height', `${position.maxHeight}px`);
      // Set cr-component's composebox max-height.
      this.setStyleVariable(
          '--cr-composebox-max-height', `${position.maxHeight}px`);
    }
    if (position.marginBottom !== null) {
      this.setStyleVariable(
          '--composebox-margin-bottom', `${position.marginBottom}px`);
    }
    if (position.marginLeft !== null) {
      this.setStyleVariable(
          '--composebox-margin-left', `${position.marginLeft}px`);
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

    // Allow downloading files. This is necessary since aim can generate images
    // for download.
    this.$.threadFrame.addEventListener('permissionrequest', (e: any) => {
      if (e.permission === 'download') {
        e.request.allow();
      }
    });

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

  protected onErrorDialogClose_() {
    this.isErrorDialogVisible_ = false;
  }

  private setIsGhostLoaderVisible(isVisible: boolean) {
    this.isGhostLoaderVisible_ = isVisible;
  }

  private hasThreadHistoryParams(url: URL): boolean {
    return url.searchParams.get('atvm') === '1' ||
        url.searchParams.get('atvm') === '3';
  }

  setPopStateFinishedCallbackForTesting(callback: () => void) {
    this.popStateFinishedCallbackForTesting_ = callback;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
