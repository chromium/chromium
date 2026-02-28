// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="not is_android">
import './composebox.js';
// </if>
import './error_dialog.js';
import './error_page.js';
import './ghost_loader.js';
import './top_toolbar.js';

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
// <if expr="not is_android">
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
// </if>
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
// <if expr="not is_android">
import type {ContextualTasksComposeboxElement} from './composebox.js';
import type {ComposeboxPosition} from './contextual_tasks.mojom-webui.js';
// </if>
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {PostMessageHandler} from './post_message_handler.js';
// <if expr="not is_android">
import type {Rect} from './post_message_handler.js';
import {getNonOccludedClipPath} from './utils/clip_path.js';
// </if>

declare global {
  interface HTMLElementEventMap {
    'loadstart': chrome.webviewTag.LoadStartEvent;
    'loadcommit': chrome.webviewTag.LoadCommitEvent;
    'newwindow': chrome.webviewTag.NewWindowEvent;
    'permissionrequest': chrome.webviewTag.PermissionRequestEvent;
  }
}

type ChromeEventFunctionType<T> =
    T extends ChromeEvent<infer ListenerType>? ListenerType : never;

export type OnBeforeRequestDetails = Parameters<
    ChromeEventFunctionType<typeof chrome.webRequest.onBeforeRequest>>[0];

// The url query parameter keys for the viewport size.
const VIEWPORT_HEIGHT_KEY = 'bih';
const VIEWPORT_WIDTH_KEY = 'biw';

// <if expr="not is_android">
// The extra padding to add to the occluders to ensure that the composebox is
// fully visible. This helps to account for inconsistencies between the bounding
// boxes of the element, and what is actually rendered (for example, box shadows
// on the elements might not be included in the bounding box).
const OCCLUDER_EXTRA_PADDING_PX = 15;
// </if>

export interface ContextualTasksAppElement {
  $: {
    threadFrame: chrome.webviewTag.WebView,
    // <if expr="not is_android">
    composebox: ContextualTasksComposeboxElement,
    // </if>
    composeboxHeaderWrapper: HTMLElement,
    composeboxHeader: HTMLElement,
    flexCenterContainer: HTMLElement,
    nameShimmer: HTMLElement,
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
      maybeShowOverlayHintText_: {type: Boolean},
      isGhostLoaderVisible_: {type: Boolean, reflect: true},
      isErrorDialogVisible_: {type: Boolean},
      enableNativeZeroStateSuggestions_: {
        type: Boolean,
        reflect: true,
      },
      enableBasicMode_: {
        type: Boolean,
        reflect: true,
      },
      enableBasicModeZOrder_: {
        type: Boolean,
        reflect: true,
      },
      isInputLocked_: {
        type: Boolean,
      },
      // <if expr="not is_android">
      forcedComposeboxBounds_: {type: Object},
      // </if>
      friendlyZeroStateGaiaName_: {type: String},
      friendlyZeroStateTitleBeforeName_: {type: String},
      friendlyZeroStateTitleAfterName_: {type: String},
      // <if expr="not is_android">
      occluders_: {type: Array},
      // </if>
    };
  }

  protected accessor friendlyZeroStateGaiaName_: string =
      loadTimeData.getString('friendlyZeroStateGaiaName');
  protected accessor friendlyZeroStateTitleBeforeName_: string =
      loadTimeData.getString('friendlyZeroStateTitleBeforeName');
  protected accessor friendlyZeroStateTitleAfterName_: string =
      loadTimeData.getString('friendlyZeroStateTitleAfterName');
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  // Whether basic mode is enabled. If disabled, isInBasicMode_,
  // isNavigatingFromAiPage_, and pendingBasicMode_ will not be updated.
  protected accessor enableBasicMode_: boolean =
      loadTimeData.getBoolean('enableBasicMode');
  protected accessor enableBasicModeZOrder_: boolean =
      loadTimeData.getBoolean('enableBasicModeZOrder');
  protected accessor isAiPage_: boolean = true;
  protected accessor isLensOverlayShowing_: boolean = false;
  protected accessor maybeShowOverlayHintText_: boolean = false;
  // Indicates if in tab mode. Most start in a tab.
  protected accessor isShownInTab_: boolean = true;
  protected accessor darkMode_: boolean = loadTimeData.getBoolean('darkMode');
  protected accessor isErrorDialogVisible_: boolean = false;
  private pendingUrl_: string = '';
  protected accessor threadTitle_: string = '';
  protected accessor isInBasicMode_: boolean = false;
  protected accessor isErrorPageVisible_: boolean = false;
  protected accessor isZeroState_: boolean = false;
  protected accessor enableNativeZeroStateSuggestions_: boolean =
      loadTimeData.getBoolean('enableNativeZeroStateSuggestions');
  protected accessor isGhostLoaderVisible_: boolean = false;
  protected accessor isInputLocked_: boolean = false;
  // The bounds of the composebox that are forced by the embedded page. These
  // bounds are relative to the <webview> and not the viewport.
  // <if expr="not is_android">
  protected accessor forcedComposeboxBounds_: Rect|null = null;
  // A list of occluders that are currently visible to the user. An occluder is
  // any element that is currently visible to the user that may be intersecting
  // and rendering over the composebox. This is a dynamic list send from the
  // embedded page, which allows the client to keep track and know which parts
  // of the composebox are not visible to the user, and therefore not clickable.
  protected accessor occluders_: Rect[]|null = null;
  // </if>

  protected friendlyZeroStateSubtitle: string =
      loadTimeData.getString('friendlyZeroStateSubtitle');
  protected friendlyZeroStateTitle: string =
      loadTimeData.getString('friendlyZeroStateTitle');
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
  // A callback to allow tests to wait until the loadstart handler in this class
  // has finished running.
  private onLoadStartFinishedCallbackForTesting_: (() => void)|null = null;
  private forceBasicModeIfOpeningThreadHistory_: boolean =
      loadTimeData.getBoolean('forceBasicModeIfOpeningThreadHistory');
  // This is needed to keep navigations between non-AIM pages from triggering
  // the input hide/restore callbacks.
  private isNavigatingFromAiPage_: boolean = false;
  // Tracks the basic mode state before a navigation occurs. This is used to
  // restore the basic mode state after the navigation, to ensure that if
  // already in basic mode, the user is returned to basic mode.

  private pendingBasicMode_: boolean|null = null;

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
        if (!this.enableBasicMode_) {
          return;
        }
        // OnBeforeRequest will trigger before the navigation, so this is needed
        // to prevent the input from being hidden when navigating to a new
        // page. However, while this guard prevents flickering, it also
        // prevents legitimate changes when going from history page to old
        // thread. Stash it. Whichever is the last basic mode signal is the
        // legitimate one.
        if (this.isNavigatingFromAiPage_) {
          this.pendingBasicMode_ = true;
          return;
        }

        this.isInBasicMode_ = true;
      }),
      callbackRouter.restoreInput.addListener(() => {
        if (!this.enableBasicMode_) {
          return;
        }
        // OnBeforeRequest will trigger before the navigation, so this is needed
        // to prevent the input from being restored when navigating to a new
        // page. However, while this guard prevents flickering, it also
        // prevents legitimate changes when going from history page to old
        // thread. Stash it. Whichever is the last basic mode signal is the
        // legitimate one.
        if (this.isNavigatingFromAiPage_) {
          this.pendingBasicMode_ = false;
          return;
        }

        this.isInBasicMode_ = false;
      }),
      // <if expr="not is_android">
      callbackRouter.injectInput.addListener(
          (title: string, thumbnail: string, fileToken: UnguessableToken) => {
            this.$.composebox.injectInput(
                title, 'chrome://image?url=' + encodeURIComponent(thumbnail),
                fileToken);
          }),
      callbackRouter.removeInjectedInput.addListener(
          (fileToken: UnguessableToken) => {
            this.$.composebox.deleteFile(fileToken);
          }),
      // </if>
      callbackRouter.setTaskDetails.addListener(updateTaskDetailsInUrl),
      callbackRouter.setAimUrl.addListener(updateAimUrl),
      callbackRouter.onZeroStateChange.addListener((isZeroState: boolean) => {
        const wasZeroState = this.isZeroState_;
        this.isZeroState_ = isZeroState;
        // If we just changed to zero state, that means
        // it is a new thread or new AIM page. Otherwise,
        // we are not in zero state anymore, or not in an AIM URL. In
        // both thread/AIM cases for zero state, we clear input.
        if (isZeroState && !wasZeroState) {
          // <if expr="not is_android">
          this.$.composebox.clearInputAndFocus();
          // Reset the forced composebox bounds since the zero state position
          // is controlled natively.
          this.forcedComposeboxBounds_ = null;
          // </if>
        }

        if (this.isZeroState_) {
          this.playZeroStateAnimations_();
        }

      }),
      callbackRouter.onLensOverlayStateChanged.addListener(
          (isOverlayShowing: boolean, maybeShowOverlayHintText: boolean) => {
            this.isLensOverlayShowing_ = isOverlayShowing;
            this.maybeShowOverlayHintText_ = maybeShowOverlayHintText;
          }),
      callbackRouter.showErrorPage.addListener(() => {
        this.isErrorPageVisible_ = true;
      }),
      callbackRouter.hideErrorPage.addListener(() => {
        this.isErrorPageVisible_ = false;
        this.maybeLoadPendingUrl_();
      }),
      callbackRouter.showOauthErrorDialog.addListener(() => {
        this.isErrorDialogVisible_ = true;
      }),
      // <if expr="not is_android">
      callbackRouter.updateComposeboxPosition.addListener(
          this.onUpdateComposeboxPosition_.bind(this),
          ),
      // </if>
      callbackRouter.lockInput.addListener(() => {
        this.isInputLocked_ = true;
      }),
      callbackRouter.unlockInput.addListener(() => {
        this.isInputLocked_ = false;
      }),
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
    this.$.threadFrame.addEventListener(
        'loadstart', this.onThreadFrameLoadStart.bind(this));
    this.$.threadFrame.addEventListener(
        'loadcommit', this.onThreadFrameLoadCommit.bind(this));
    this.$.threadFrame.addEventListener(
        'contentload', this.onThreadFrameContentLoad.bind(this));
    this.$.threadFrame.addEventListener(
        'loadabort', this.onThreadFrameLoadAbort.bind(this));

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
      // <if expr="not is_android">
      this.$.composebox.clearInputAndFocus();
      // </if>
    }

    const threadUrlAsUrl = new URL(threadUrl);
    // If the thread URL has parameters to open history, set basic mode.
    if (this.enableBasicMode_ && this.hasThreadHistoryParams(threadUrlAsUrl) &&
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

    if (taskUuid) {
      const {isPendingErrorPage} =
          await this.browserProxy_.handler.isPendingErrorPage(
              {value: taskUuid});
      this.isErrorPageVisible_ = isPendingErrorPage;
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
    this.removeWebviewRequestOverrides();
    this.eventTracker_.removeAll();
  }

  override firstUpdated() {
    this.postMessageHandler_ =
        new PostMessageHandler(this.$.threadFrame, this.browserProxy_);
    // <if expr="not is_android">
    this.postMessageHandler_.setInputPlateBoundsUpdateCallback(
        this.onInputPlateBoundsUpdate_.bind(this));

    this.eventTracker_.add(
        this.$.composebox, 'composebox-height-update', (e: CustomEvent) => {
          // TODO(crbug.com/483737358): Sending an object instead of a proto is
          // a temporary solution to unblock the prototype. Remove this method
          // once the proto is implemented on the webview side.
          this.postMessageHandler_.sendObjectMessage({
            type: 'composebox-height-update',
            height: e.detail.height,
          });
          // Update the height of the forced composebox bounds if it is set.
          if (this.forcedComposeboxBounds_) {
            this.forcedComposeboxBounds_.height = e.detail.height;
          }
        });
    // </if>
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

  private async playZeroStateAnimations_() {
    await this.updateComplete;

    const restartAnimations = (element: HTMLElement) => {
      element.getAnimations().forEach(animation => {
        animation.cancel();
        animation.play();
      });
    };

    // <if expr="not is_android">
    restartAnimations(this.$.composebox);
    // </if>
    restartAnimations(this.$.composeboxHeaderWrapper);

    if (this.$.nameShimmer) {
      restartAnimations(this.$.nameShimmer);
    }

    // Restart the composebox glow animation.
    // <if expr="not is_android">
    this.$.composebox.startExpandAnimation();
    // </if>
  }

  // <if expr="not is_android">
  private setStyleVariable(variable: string, value: string) {
    this.$.composebox.style.setProperty(variable, `${value}px`);
  }
  // </if>

  private async onThreadFrameLoadStart(ev: chrome.webviewTag.LoadStartEvent) {
    // If is from inner iframe and not from main webview URL:
    if (!ev.isTopLevel) {
      return;
    }
    // <if expr="not is_android">
    // Reset the composebox bounds and the occluders since the embedded page is
    // reloading.
    this.forcedComposeboxBounds_ = null;
    this.occluders_ = null;
    // </if>

    // Set frame loading to true initially to avoid race conditions.
    this.isFrameLoading = true;
    const wasAiPage = this.isAiPage_;
    const {isAiPage} = await this.browserProxy_.handler.isAiPage(ev.url);

    // If the frame is no longer loading after waiting for isAiPage,
    // then exit early to prevent racind.
    if (!this.isFrameLoading) {
      if (this.onLoadStartFinishedCallbackForTesting_) {
        this.onLoadStartFinishedCallbackForTesting_();
      }
      return;
    }

    if (!isAiPage) {
      // If this is not an AI page, show the ghost loader.
      this.setIsGhostLoaderVisible(true);
    } else if (this.enableBasicMode_ && wasAiPage) {
      // Since this is a navigation from one AI page to another,
      // enter basic mode to avoid flickering between navigations.
      this.isNavigatingFromAiPage_ = true;
      // In the case where this basic mode is currently set to false,
      // set the pending basic mode to false right away to avoid flickering when
      // loading the first AIM page (like from a contextual query that directly
      // opens the side panel). Without this call, the first loaded AIM page
      // will load with basic mode enabled and will wait for the AIM page to
      // load and then complete the handshake in order to re-disable basic mode.
      if (!this.isInBasicMode_) {
        this.pendingBasicMode_ = false;
      }
      this.isInBasicMode_ = true;
    }

    if (this.onLoadStartFinishedCallbackForTesting_) {
      this.onLoadStartFinishedCallbackForTesting_();
    }
  }

  private onThreadFrameLoadCommit(ev: chrome.webviewTag.LoadCommitEvent) {
    // If is from inner iframe and not from main webview URL:
    if (!ev.isTopLevel) {
      return;
    }
    this.updateBasicModeAfterNavigation();
  }

  private onThreadFrameContentLoad() {
    this.isFrameLoading = false;
    this.setIsGhostLoaderVisible(false);
    this.updateBasicModeAfterNavigation();
  }

  private onThreadFrameLoadAbort() {
    this.isFrameLoading = false;
    this.setIsGhostLoaderVisible(false);
    this.updateBasicModeAfterNavigation();
  }

  /* Adjust composebox based on server notifications. Negatives are used if
   * server wants to change marginTop, marginRight.
   */
  // <if expr="not is_android">
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
  // </if>

  // <if expr="not is_android">
  private onInputPlateBoundsUpdate_(inputRect?: Rect, occluders?: Rect[]) {
    if (inputRect !== undefined) {
      const currentHeight = this.$.composebox.offsetHeight;
      if (currentHeight !== inputRect.height) {
        // If the height that the client reports for the composebox is different
        // from the height that the server is reporting, update the server.
        this.postMessageHandler_.sendObjectMessage({
          type: 'composebox-height-update',
          height: currentHeight,
        });
      }
      this.forcedComposeboxBounds_ = inputRect;
      this.forcedComposeboxBounds_.height = currentHeight;
    }
    if (occluders !== undefined) {
      this.occluders_ = occluders;
    }
  }

  getComposeboxBoundsStyles() {
    if (this.isZeroState_ || !this.forcedComposeboxBounds_) {
      return '';
    }

    // Since this.forcedComposeboxBounds_ is relative to the <webview>, and
    // the composebox is relative to the viewport, adjust the bounds to be
    // relative to the viewport.
    const frameRect = this.$.threadFrame.getBoundingClientRect();
    const relativeRect = {
      top: frameRect.top + this.forcedComposeboxBounds_.top,
      left: frameRect.left + this.forcedComposeboxBounds_.left,
      width: this.forcedComposeboxBounds_.width,
      height: this.forcedComposeboxBounds_.height,
      right: frameRect.left + this.forcedComposeboxBounds_.right,
      bottom: frameRect.top + this.forcedComposeboxBounds_.bottom,
    };

    // Do not set height, since the expanding of the composebox is dynamic.
    // Set the bottom of the rect instead of the top to allow the composebox to
    // expand upwards.
    const style: string[] = [
      `--composebox-margin-bottom: 0;`,  // Need to remove margin on the child
                                         // container.
      `position: relative;`,
      `bottom: ${window.innerHeight - relativeRect.bottom}px;`,
      `left: ${relativeRect.left}px;`,
      `width: ${relativeRect.width}px;`,
      `margin: 0;`,
      `max-width: none;`,
      `min-width: 0;`,
    ];
    return style.join(' ');
  }
  // </if>

  getThreadFrameStyles(): string {
    // <if expr="not is_android">
    if (this.occluders_ == null) {
      return '';
    }

    // If the forced composebox bounds are set, use those since its cheaper
    // than calling getBoundingClientRect();
    const composeboxBounds = this.forcedComposeboxBounds_ ??
        this.$.composebox.getBoundingClientRect();

    // If occluders are present, set the clip path and a z-index that ensures
    // the thread frame is above the occluders.
    return getNonOccludedClipPath(
               composeboxBounds, this.occluders_, OCCLUDER_EXTRA_PADDING_PX) +
        'z-index: 100;';
    // </if>
    // <if expr="is_android">
    return '';
    // </if>
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
    // <if expr="not is_android">
    this.$.composebox.startExpandAnimation();
    this.$.composebox.clearInputAndFocus();
    // </if>
  }

  getEnableNativeZeroStateSuggestionsForTesting() {
    return this.enableNativeZeroStateSuggestions_;
  }

  setEnableNativeZeroStateSuggestionsForTesting(enable: boolean) {
    this.enableNativeZeroStateSuggestions_ = enable;
  }

  // Conditionally update the provided thread URL so it restores an existing
  // thread. If the thread URL already contains the params for loading a
  // specific thread, this will return the same URL that was provided.
  private maybeUpdateThreadUrlForRestore(threadUrl: URL, webUiUrl: URL):
      string {
    // Check if the provided URL is default by checking for thread ID, turn
    // ID, and title. If those params are not present, but are present on the
    // WebUI URL, apply them to the thread URL.
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
    // If all the data needed to make the initial request is available, load
    // the pending URL.
    if (this.pendingUrl_ && this.commonSearchParams_ &&
        !this.isErrorPageVisible_) {
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
    this.$.threadFrame.addEventListener(
        'permissionrequest', (e: chrome.webviewTag.PermissionRequestEvent) => {
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

  private removeWebviewRequestOverrides() {
    this.$.threadFrame.request.onBeforeRequest.removeListener(
        this.onBeforeRequest);
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

  private updateBasicModeAfterNavigation() {
    if (!this.enableBasicMode_ || !this.isNavigatingFromAiPage_) {
      return;
    }
    // If basic mode was changed while loading the
    // thread frame, utilize that new value instead.
    if (this.pendingBasicMode_ !== null) {
      this.isInBasicMode_ = this.pendingBasicMode_;
    }

    this.isNavigatingFromAiPage_ = false;
    this.pendingBasicMode_ = null;
  }

  private setIsGhostLoaderVisible(isVisible: boolean) {
    if (this.enableGhostLoader_) {
      this.isGhostLoaderVisible_ = isVisible;
    }
  }

  private hasThreadHistoryParams(url: URL): boolean {
    return url.searchParams.get('atvm') === '1' ||
        url.searchParams.get('atvm') === '3';
  }

  setPopStateFinishedCallbackForTesting(callback: () => void) {
    this.popStateFinishedCallbackForTesting_ = callback;
  }

  setOnLoadStartFinishedCallbackForTesting(callback: () => void) {
    this.onLoadStartFinishedCallbackForTesting_ = callback;
  }

  setMockPostMessageHandlerForTesting(
      mockPostMessageHandler: PostMessageHandler) {
    this.postMessageHandler_ = mockPostMessageHandler;
  }

  isNavigatingForTesting() {
    return this.isNavigatingFromAiPage_;
  }

  onBeforeRequestForTesting(details: OnBeforeRequestDetails) {
    return this.onBeforeRequest(details);
  }

  getIsFrameLoadingForTesting() {
    return this.isFrameLoading;
  }

  onThreadFrameLoadStartForTesting(event: chrome.webviewTag.LoadStartEvent) {
    this.onThreadFrameLoadStart(event);
  }

  onThreadFrameContentLoadForTesting() {
    this.onThreadFrameContentLoad();
  }

  onThreadFrameLoadAbortForTesting() {
    this.onThreadFrameLoadAbort();
  }

  setIsZeroStateForTesting(isZeroState: boolean) {
    this.isZeroState_ = isZeroState;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
