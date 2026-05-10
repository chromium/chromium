// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="not is_android">
import './composebox.js';
import './onboarding_tooltip.js';

import type {ContextualTasksComposeboxElement} from './composebox.js';
import type {ContextualTasksOnboardingTooltipElement} from './onboarding_tooltip.js';
// </if>

// <if expr="is_android">
// ContextualTasksComposeboxElement is not compiled on Android.
type ContextualTasksComposeboxElement = any;
// </if>

import './error_dialog.js';
import './error_page.js';
import './ghost_loader.js';
import './top_toolbar.js';

import {isFullWebView} from './web_view_type.js';
import type {LoadAbortEvent, LoadEvent, NewWindowEvent, PermissionRequestEvent, WebViewType} from './web_view_type.js';
import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {ComposeboxPosition, InjectedInput} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {PostMessageHandler} from './post_message_handler.js';
import type {Rect} from './post_message_handler.js';
import {getNonOccludedClipPath} from './utils/clip_path.js';
import {recordAction} from './utils.js';

declare global {
  interface HTMLElementEventMap {
    'loadstart': chrome.webviewTag.LoadStartEvent|LoadEvent;
    'loadredirect': chrome.webviewTag.LoadRedirectEvent;
    'loadabort': chrome.webviewTag.LoadAbortEvent|LoadAbortEvent;
    'loadcommit': chrome.webviewTag.LoadCommitEvent|LoadEvent;
    'newwindow': chrome.webviewTag.NewWindowEvent|NewWindowEvent;
    'permissionrequest': chrome.webviewTag.PermissionRequestEvent|
        PermissionRequestEvent;
  }
}

type ChromeEventFunctionType<T> =
    T extends ChromeEvent<infer ListenerType>? ListenerType : never;

export type OnBeforeRequestDetails = Parameters<
    ChromeEventFunctionType<typeof chrome.webRequest.onBeforeRequest>>[0];

// The url query parameter keys for the viewport size.
const VIEWPORT_HEIGHT_KEY = 'bih';
const VIEWPORT_WIDTH_KEY = 'biw';

const CHROME_TASK_PARAM_KEY = 'chrome_task_id';
const DEBUG_PARAM_KEY = 'deb';
const CHROME_HOST_PARAM_KEY = 'chrome_host';

const AIOH_URL_IDENTIFIER = 'aioh';

// The extra padding to add to the occluders to ensure that the composebox is
// fully visible. This helps to account for inconsistencies between the bounding
// boxes of the element, and what is actually rendered (for example, box shadows
// on the elements might not be included in the bounding box).
const OCCLUDER_EXTRA_PADDING_PX = 15;

// LINT.IfChange(ComposeboxBorderRadius)
const COMPOSEBOX_BORDER_RADIUS_PX = 24;
// LINT.ThenChange(//depot/chromium/chrome/browser/resources/contextual_tasks/composebox.css:ComposeboxBorderRadius)

export interface ContextualTasksAppElement {
  $: {
    threadFrame: WebViewType,
    composeboxHeaderWrapper: HTMLElement,
    composeboxHeader: HTMLElement,
    flexCenterContainer: HTMLElement,
    nameShimmer: HTMLElement,
    // <if expr="not is_android">
    composebox: ContextualTasksComposeboxElement,
    onboardingTooltip?: ContextualTasksOnboardingTooltipElement,
    // </if>
  };
}

// Updates the param for task ID in the URL and adds an entry in history if it
// changed.
function updateTaskDetailsInUrl(taskId: Uuid) {
  const url = new URL(window.location.href);

  const existingTaskId = url.searchParams.get(CHROME_TASK_PARAM_KEY);
  url.searchParams.set(CHROME_TASK_PARAM_KEY, taskId.value);

  // Allow back navigation if the task ID changes. Other changes to the URL
  // represent state changes for the current task.
  if (existingTaskId !== taskId.value) {
    window.history.pushState({}, '', url.href);
  } else {
    window.history.replaceState({}, '', url.href);
  }
}

// Copy the params from the current aim URL to the webui URL without adding a
// history entry. Keeping the params in sync ensures the page reloads or
// restores correctly.
function updateWebuiParams(aimUrl: Url) {
  const webuiUrl = new URL(window.location.href);

  const taskId = webuiUrl.searchParams.get(CHROME_TASK_PARAM_KEY);
  const host = webuiUrl.searchParams.get(CHROME_HOST_PARAM_KEY);

  // Clear the existing params
  webuiUrl.search = '';

  // Preserve host if present in current URL.
  if (host) {
    webuiUrl.searchParams.set(CHROME_HOST_PARAM_KEY, host);
  }

  // Add all the params from the aim URL, except host.
  new URL(aimUrl).searchParams.forEach((value, key) => {
    if (key !== CHROME_HOST_PARAM_KEY) {
      webuiUrl.searchParams.set(key, value);
    }
  });

  // Add the task ID back to the params if it was there to begin with.
  if (taskId) {
    webuiUrl.searchParams.set(CHROME_TASK_PARAM_KEY, taskId);
  }

  window.history.replaceState({}, '', webuiUrl.href);
}

// Returns whether the value of the "deb" param contains "nocobrowse1" which
// should cause the user to be removed from the cobrowse ui.
function hasExitCobrowseParam(url: URL): boolean {
  const debParam = url.searchParams.get(DEBUG_PARAM_KEY) || '';
  return debParam.indexOf('nocobrowse1') > -1;
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
      isInputHidden_: {type: Boolean, reflect: true},
      // Means no queries have been submitted in current AIM thread.
      isZeroState_: {
        type: Boolean,
        reflect: true,
      },
      inNlm_: {
        type: Boolean,
        reflect: true,
      },
      // Whether top level navigation was aborted. One example where this can
      // occur is if a user is offline.
      isLoadError_: {
        type: Boolean,
        reflect: true,
      },
      isAiPage_: {type: Boolean, reflect: true},
      isLensOverlayShowing_: {type: Boolean},
      isOverlayOpenForAimVisualSearch_: {type: Boolean},
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
      useStratusDarkModeColors_: {
        type: Boolean,
        reflect: true,
      },
      isInputLocked_: {
        type: Boolean,
      },
      isLoadingZeroStateFromResults_: {
        type: Boolean,
        reflect: true,
      },
      forcedComposeboxBounds_: {type: Object},
      userName_: {type: String},
      friendlyZeroStateTitleBeforeName_: {type: String},
      friendlyZeroStateTitleAfterName_: {type: String},
      friendlyZeroStateTitle: {type: String},
      friendlyZeroStateSubtitle: {type: String},
      occluders_: {type: Array},
      showOnboardingTooltip_: {
        type: Boolean,
        value: loadTimeData.getBoolean('showOnboardingTooltip'),
      },
      energyEffectEnabled_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected accessor energyEffectEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectEnabled');
  protected accessor showOnboardingTooltip_: boolean =
      loadTimeData.getBoolean('showOnboardingTooltip');
  protected accessor userName_: string =
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
  private nlmUrlParam_: string = loadTimeData.getString('nlmUrlParam');
  private enableCustomNlmUi_: boolean =
      loadTimeData.getBoolean('enableCustomNlmUi');
  // Whether top-level navigation failed. Initialized based on online status
  // though top-level navigation could fail for numerous reasons.
  protected accessor isLoadError_: boolean = !window.navigator.onLine;
  protected accessor isAiPage_: boolean = loadTimeData.getBoolean('isAiPage');
  protected accessor isLensOverlayShowing_: boolean = false;
  protected accessor isOverlayOpenForAimVisualSearch_: boolean = false;
  // Indicates if in tab mode. Most start in a tab.
  protected accessor isShownInTab_: boolean = true;
  protected accessor darkMode_: boolean = loadTimeData.getBoolean('darkMode');
  protected accessor isErrorDialogVisible_: boolean = false;
  private pendingUrl_: string = '';
  protected accessor threadTitle_: string = '';
  protected accessor isInBasicMode_: boolean = false;
  protected accessor isInputHidden_: boolean = false;

  protected accessor isErrorPageVisible_: boolean = false;
  // Whether no queries have been submitted in the current AIM thread. This
  // can be undefined on initial load to prevent the composebox from flashing
  // briefly before the zero state is rendered.
  protected accessor isZeroState_: boolean|undefined =
      loadTimeData.getBoolean('isGhostLoaderVisible') ? false : undefined;
  protected accessor enableNativeZeroStateSuggestions_: boolean =
      loadTimeData.getBoolean('enableNativeZeroStateSuggestions');
  protected accessor inNlm_: boolean = false;
  protected accessor isGhostLoaderVisible_: boolean =
      loadTimeData.getBoolean('isGhostLoaderVisible');
  protected accessor useStratusDarkModeColors_: boolean =
      loadTimeData.getBoolean('useStratusDarkModeColors');
  protected accessor isInputLocked_: boolean = false;
  protected accessor isLoadingZeroStateFromResults_: boolean = false;
  // The bounds of the composebox that are forced by the embedded page. These
  // bounds are relative to the <webview> and not the viewport.
  protected accessor forcedComposeboxBounds_: Rect|null = null;
  // A list of occluders that are currently visible to the user. An occluder is
  // any element that is currently visible to the user that may be intersecting
  // and rendering over the composebox. This is a dynamic list send from the
  // embedded page, which allows the client to keep track and know which parts
  // of the composebox are not visible to the user, and therefore not clickable.
  protected accessor occluders_: Rect[]|null = null;

  protected accessor friendlyZeroStateSubtitle: string =
      loadTimeData.getString('friendlyZeroStateSubtitle');
  protected accessor friendlyZeroStateTitle: string =
      loadTimeData.getString('friendlyZeroStateTitle');
  // Tracks whether the frame is currently loading. Needed to avoid race
  // condition while awaiting isAiPage.
  private isFrameLoading: boolean = false;
  private listenerIds_: number[] = [];
  private eventTracker_: EventTracker = new EventTracker();
  private commonSearchParams_: {[key: string]: string}|null = null;
  private postMessageHandler_: PostMessageHandler|null = null;
  private signInDomains_: string[] =
      loadTimeData.getString('contextualTasksSignInDomains').split(',');
  private host_: string|null = null;
  // Whether the composebox jump fix is enabled. This fix hides the composebox
  // until the server gives the embedded page gives the initial bounds for the
  // composebox.
  private enableComposeboxJumpFix_: boolean =
      loadTimeData.getBoolean('enableComposeboxJumpFix');
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
  // Tracks the last thread frame load start event. Is null if the thread frame
  // is not currently loading. This is used to track if the thread frame
  // actually loaded or if the load was aborted. Without this, we can't
  // distinguish between the two cases, since load start will always be called
  // even if the load is aborted and the frame therefore never changes.
  private lastThreadFrameLoadStartEvent_: chrome.webviewTag.LoadStartEvent|
      LoadEvent|null = null;

  private updateThemeFromUrl(url: URL) {
    const csParam = url.searchParams.get('cs');
    if (csParam === '0') {
      this.darkMode_ = false;
    } else if (csParam === '1') {
      this.darkMode_ = true;
    }
    this.updateBackgroundColor_();
    this.updateCommonSearchParams();
  }
  private get composebox_(): ContextualTasksComposeboxElement|null {
    // <if expr="not is_android">
    return this.$.composebox || null;
    // </if>
    // <if expr="is_android">
    return null;
    // </if>
  }

  override async connectedCallback() {
    super.connectedCallback();
    this.updateBackgroundColor_();

    // Record the WebUI URL in case one of the events below fires and changes
    // it.
    const webUiUrlOnLoad = new URL(window.location.href);
    this.host_ = webUiUrlOnLoad.searchParams.get(CHROME_HOST_PARAM_KEY);
    if (!this.host_ && loadTimeData.valueExists('chrome_host')) {
      this.host_ = loadTimeData.getString('chrome_host');
    }
    // Relying on C++ to provide the correct host via getUrlForTask

    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [
      callbackRouter.onSidePanelStateChanged.addListener(
          () => this.updateSidePanelState()),
      callbackRouter.setThreadTitle.addListener(title => {
        this.threadTitle_ = title;
        document.title = title || loadTimeData.getString('title');
      }),
      callbackRouter.onAiPageStatusChanged.addListener(isAiPage => {
        this.isAiPage_ = isAiPage;
      }),
      callbackRouter.postMessageToWebview.addListener(
          this.postMessageToWebview.bind(this)),
      callbackRouter.onHandshakeComplete.addListener(
          this.onHandshakeComplete.bind(this)),

      // TODO(crbug.com/474359572): Rename this to be more descriptive of what
      // it actually does.
      callbackRouter.hideInput.addListener(() => {
        this.isInputHidden_ = true;
      }),
      callbackRouter.restoreInput.addListener(() => {
        this.isInputHidden_ = false;
      }),
      callbackRouter.enterBasicMode.addListener(() => {
        if (!this.enableBasicMode_) {
          return;
        }
        if (this.isNavigatingFromAiPage_) {
          this.pendingBasicMode_ = true;
          return;
        }
        this.isInBasicMode_ = true;
      }),
      callbackRouter.exitBasicMode.addListener(() => {
        if (!this.enableBasicMode_) {
          return;
        }
        if (this.isNavigatingFromAiPage_) {
          this.pendingBasicMode_ = false;
          return;
        }
        this.isInBasicMode_ = false;
      }),
      callbackRouter.injectInput.addListener(async (input: InjectedInput) => {
        await this.composebox_?.injectInput(input);
      }),
      callbackRouter.removeInjectedInput.addListener(fileToken => {
        this.composebox_?.deleteFile(fileToken);
      }),
      callbackRouter.setTaskDetails.addListener(updateTaskDetailsInUrl),
      callbackRouter.setAimUrl.addListener(updateWebuiParams),
      callbackRouter.onZeroStateChange.addListener(isZeroState => {
        this.isZeroState_ = isZeroState;
        // If we just changed to zero state, that means
        // it is a new thread or new AIM page. Otherwise,
        // we are not in zero state anymore, or not in an AIM URL. In
        // both thread/AIM cases for zero state, we clear input.
        if (isZeroState) {
          this.composebox_?.clearInputAndFocus();
          // Reset the forced composebox bounds since the zero state position
          // is controlled natively.
          this.forcedComposeboxBounds_ = null;
        }
      }),
      callbackRouter.setInNlm.addListener((inNlm: boolean) => {
        this.inNlm_ = inNlm;
      }),
      callbackRouter.onLensOverlayStateChanged.addListener(
          (isOverlayShowing, isOverlayOpenForAimVisualSearch) => {
            this.isLensOverlayShowing_ = isOverlayShowing;
            this.isOverlayOpenForAimVisualSearch_ =
                isOverlayOpenForAimVisualSearch;
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
      callbackRouter.updateComposeboxPosition.addListener(
          this.onUpdateComposeboxPosition_.bind(this),
          ),
      callbackRouter.lockInput.addListener(() => {
        this.isInputLocked_ = true;
      }),
      callbackRouter.unlockInput.addListener(() => {
        this.isInputLocked_ = false;
      }),
    ];

    // Track the tooltip visibility events fired from the composebox.
    this.eventTracker_.add(
        window, 'update-tooltip-visibility',
        () => this.updateTooltipVisibility_());

    this.eventTracker_.add(window, 'popstate', async () => {
      // The back button may pop state that was pushed by a task change. If that
      // is the case, fetch the URL for the task ID and load that in the frame.
      const taskUuid =
          new URLSearchParams(location.search).get(CHROME_TASK_PARAM_KEY);
      if (taskUuid) {
        const {url} =
            await this.browserProxy_.handler.getUrlForTask({value: taskUuid});

        // Do nothing if the app element is no longer attached to the page. This
        // can occur in tests where awaiting the call above will delay the rest
        // of this handler and affect other tests in the suite.
        if (!this.isConnected) {
          return;
        }

        if (this.isShownInTab_) {
          recordAction(
              'ContextualTasks.HistoryNavigation.UserAction.NavigatedInFullTab');
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
        'loadredirect', this.onThreadFrameLoadRedirect.bind(this));
    this.$.threadFrame.addEventListener(
        'loadabort', this.onThreadFrameLoadAbort.bind(this));
    this.$.threadFrame.addEventListener(
        'loadcommit', this.onThreadFrameLoadCommit.bind(this));
    this.$.threadFrame.addEventListener(
        'contentload', this.onThreadFrameContentLoad.bind(this));

    // Setup the webview request overrides before loading the first URL.
    this.setupWebviewRequestOverrides();

    // Check if the URL that loaded this page has a task attached to it. If it
    // does, we'll use the tasks URL to load the embedded page.
    const taskUuid = webUiUrlOnLoad.searchParams.get(CHROME_TASK_PARAM_KEY);
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
      this.composebox_?.clearInputAndFocus();
    }

    const threadUrlAsUrl = new URL(threadUrl);
    this.updateThemeFromUrl(threadUrlAsUrl);
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

    // Allow URLs with the debug param set to exit the webui. This param is
    // the same one used to prevent aim urls from entering the webui, so when
    // set, it will be attached to the thread URL which will keep the user out
    // of this UI.
    if (hasExitCobrowseParam(webUiUrlOnLoad)) {
      window.location.href = threadUrlAsUrl.href;
      return;
    }

    // Check if the initial render should be zero state.
    const {isZeroState} =
        await this.browserProxy_.handler.isZeroState(threadUrlAsUrl.href);
    this.isZeroState_ = isZeroState;

    this.inNlm_ = this.checkInNlm_(threadUrlAsUrl);

    // The thread URL is considered pending (not loaded immediately in the
    // webview) until oauth tokens are received from the WebUI controller. This
    // prevents situations where the user is technically signed out of the
    // embedded frame and unable to save or access existing data.
    this.pendingUrl_ = threadUrlAsUrl.href;
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

    this.updateTooltipVisibility_();

    const composebox = this.composebox_;
    if (!composebox) {
      return;
    }

    this.postMessageHandler_.setInputPlateBoundsUpdateCallback(
        this.onInputPlateBoundsUpdate_.bind(this));

    this.postMessageHandler_.setInputPlateBoundsUpdateCallback(
        this.onInputPlateBoundsUpdate_.bind(this));

    this.eventTracker_.add(
        composebox, 'composebox-height-update',
        (e: CustomEvent<{height: number}>) => {
          // TODO(crbug.com/483737358): Sending an object instead of a proto
          // is a temporary solution to unblock the prototype. Remove this
          // method once the proto is implemented on the webview side.
          assert(this.postMessageHandler_);
          this.postMessageHandler_.sendObjectMessage({
            type: 'composebox-height-update',
            height: e.detail.height,
          });
          // Update the height of the forced composebox bounds if it is set.
          if (this.forcedComposeboxBounds_) {
            this.forcedComposeboxBounds_ = {
              ...this.forcedComposeboxBounds_,
              height: e.detail.height,
              top: this.forcedComposeboxBounds_.bottom - e.detail.height,
            };
          } else {
            this.requestUpdate();
          }
        });
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

  private updateTooltipVisibility_() {
    // Tooltip not supported on Android. Therefore, make calls to this method
    // a no-op.
    // <if expr="not is_android">
    const tooltip = this.$.onboardingTooltip;
    const composeboxContainer = this.composebox_;
    if (!composeboxContainer) {
      return;
    }
    const crComposebox = this.composebox_.getComposebox();
    if (tooltip && crComposebox) {
      tooltip.updateTooltipVisibility(composeboxContainer, crComposebox);
    }
    // </if>
  }

  private async playZeroStateAnimations_() {
    await this.updateComplete;
    const restartAnimations = (element: HTMLElement) => {
      element.getAnimations().forEach(animation => {
        animation.cancel();
        animation.play();
      });
    };

    const composebox = this.composebox_;
    if (composebox) {
      restartAnimations(composebox);
      // Restart the composebox glow animation.
      composebox.startExpandAnimation();
    }
    restartAnimations(this.$.composeboxHeaderWrapper);

    const nameShimmer = this.shadowRoot.getElementById('nameShimmer');
    if (nameShimmer) {
      restartAnimations(nameShimmer);
    }
  }

  private setStyleVariable(variable: string, value: string) {
    this.composebox_?.style.setProperty(variable, `${value}px`);
  }

  private onThreadFrameLoadStart(e: Event) {
    const ev = e as chrome.webviewTag.LoadStartEvent | LoadEvent;
    // If is from inner iframe and not from main webview URL:
    if (!ev.isTopLevel) {
      return;
    }

    // If a thread URL is loaded with the debug param to exit coborowse,
    // navigate the tab to that URL.
    if (hasExitCobrowseParam(new URL(ev.url))) {
      window.location.href = ev.url;
      return;
    }

    this.isLoadError_ = !window.navigator.onLine;

    // Stash the last thread frame load start event. This will be used once the
    // navigation is determined to have aborted or not.
    this.lastThreadFrameLoadStartEvent_ = ev;
  }

  private onThreadFrameLoadRedirect(e: Event) {
    const ev = e as chrome.webviewTag.LoadRedirectEvent;
    // If is from inner iframe and not from main webview URL:
    if (!ev.isTopLevel) {
      return;
    }

    this.maybeOnThreadFrameTopLevelNavigation(ev.oldUrl);
  }

  private onThreadFrameLoadCommit(e: Event) {
    const ev = e as chrome.webviewTag.LoadCommitEvent | LoadEvent;
    // If is from inner iframe and not from main webview URL:
    if (!ev.isTopLevel) {
      return;
    }
    this.updateBasicModeAfterNavigation();
    this.maybeOnThreadFrameTopLevelNavigation(ev.url);
  }

  private onThreadFrameContentLoad() {
    this.isFrameLoading = false;
    this.isLoadingZeroStateFromResults_ = false;
    this.setIsGhostLoaderVisible(false);
    this.updateBasicModeAfterNavigation();
  }

  private async onThreadFrameLoadAbort(ev: Event) {
    const e = ev as chrome.webviewTag.LoadAbortEvent | LoadAbortEvent;
    // It is possible for a redirect to abort a load before committing. To
    // prevent ghost loader flickers in this case, only hide the ghost loader if
    // the frame was previously set to loading.
    if (this.isFrameLoading) {
      this.setIsGhostLoaderVisible(false);
    }
    this.isFrameLoading = false;
    this.isLoadingZeroStateFromResults_ = false;

    // The navigation aborted, so reset the last thread frame load start event,
    // since the frame is no longer loading. Without this, every
    // onThreadFrameLoadStart event will be treated as a navigation even when
    // the navigation has aborted.
    this.lastThreadFrameLoadStartEvent_ = null;

    // Navigations delegated to external applications fire a load abort event.
    // Check if the embedded page is showing an error document.
    if (e.isTopLevel) {
      const {isErrorDocument} =
          await this.browserProxy_.handler.isEmbeddedPageErrorDocument();
      if (isErrorDocument) {
        // TODO(crbug.com/489713572): Potentially query autocomplete when the
        // error is resolved and the page reloads
        this.isLoadError_ = true;
      }
    }
    this.updateBasicModeAfterNavigation();
  }

  private maybeOnThreadFrameTopLevelNavigation(navigationUrl: string) {
    // Since the navigation has redirected/committed instead of aborted, threat
    // this as a top level navigation. Reset the last thread frame load start
    // event to prevent calling onThreadFrameTopLevelNavigation again.
    if (this.lastThreadFrameLoadStartEvent_ &&
        this.lastThreadFrameLoadStartEvent_.url === navigationUrl) {
      const event = this.lastThreadFrameLoadStartEvent_;
      this.lastThreadFrameLoadStartEvent_ = null;
      const url = new URL(navigationUrl);
      this.updateThemeFromUrl(url);
      this.onThreadFrameTopLevelNavigation(event);
    }
  }

  private async onThreadFrameTopLevelNavigation(e: Event) {
    const ev = e as chrome.webviewTag.LoadStartEvent | LoadEvent;
    // Reset the composebox bounds and the occluders since the embedded page is
    // reloading.
    this.forcedComposeboxBounds_ = null;
    this.occluders_ = null;
    this.isInputHidden_ = false;

    // Set frame loading to true initially to avoid race conditions.
    this.isFrameLoading = true;
    const wasAiPage = this.isAiPage_;
    const wasZeroState = this.isZeroState_;

    this.composebox_?.setToolFromUrl(ev.url);

    const {isAiPage} = await this.browserProxy_.handler.isAiPage(ev.url);
    const {isZeroState} = await this.browserProxy_.handler.isZeroState(ev.url);

    // If the frame is no longer loading after waiting for isAiPage,
    // then exit early to prevent racind.
    if (!this.isFrameLoading) {
      if (this.onLoadStartFinishedCallbackForTesting_) {
        this.onLoadStartFinishedCallbackForTesting_();
      }
      return;
    }

    if (isAiPage && isZeroState) {
      this.isZeroState_ = true;
      this.playZeroStateAnimations_();
    }

    if (!wasZeroState && isZeroState) {
      this.isLoadingZeroStateFromResults_ = true;
    }

    if (!isAiPage) {
      // If this is not an AI page, show the ghost loader.
      // Update the isAiPage_ property so the ghost loader doesn't jump when
      // the property is updated later.
      this.isAiPage_ = isAiPage;
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

  private onInputPlateBoundsUpdate_(inputRect?: Rect, occluders?: Rect[]) {
    if (inputRect !== undefined) {
      const composebox = this.composebox_!;
      const currentHeight = composebox.offsetHeight;
      const currentUrl = this.$.threadFrame.src;
      if (currentUrl.includes(AIOH_URL_IDENTIFIER) &&
          this.forcedComposeboxBounds_ === null) {
        this.playComposeboxAiohFadeInAnimation_();
      }
      if (currentHeight !== inputRect.height) {
        // If the height that the client reports for the composebox is different
        // from the height that the server is reporting, update the server.
        assert(this.postMessageHandler_);
        this.postMessageHandler_.sendObjectMessage({
          type: 'composebox-height-update',
          height: currentHeight,
        });
      }
      // Since the height is controlled client side and the composebox grows
      // updwards, set the top of the rect to match the current height to avoid
      // miscalculations in the clip path.
      this.forcedComposeboxBounds_ = {
        ...inputRect,
        height: currentHeight,
        top: inputRect.bottom - currentHeight,
      };
    }
    if (occluders !== undefined) {
      this.occluders_ = occluders;
    }
  }

  private playComposeboxAiohFadeInAnimation_() {
    const composebox = this.composebox_;
    if (!composebox) {
      return;
    }
    composebox.animate(
        [
          {opacity: 0},
          {opacity: 1},
        ],
        {
          duration: 150,
          easing: 'ease-in-out',
          fill: 'forwards',
        });
  }

  protected isComposeboxHidden_(): boolean {
    // Stay hidden until the first isZeroState_ value is determined to prevent
    // the composebox from flickering in.
    if (this.isZeroState_ === undefined) {
      return true;
    }

    if (this.isInputHidden_) {
      return true;
    }

    // If using the basic mode without z-ordering, if in basic mode, hide the
    // composebox.
    if (this.enableBasicMode_ && this.isInBasicMode_ &&
        !this.enableBasicModeZOrder_) {
      return true;
    }

    // If in NLM mode, only show the composebox if the forcedComposeboxBounds
    // are set. We expect NLM mode to send us bounds.
    if (this.inNlm_ && !this.forcedComposeboxBounds_) {
      return true;
    }

    // If on an AI page and not the zero state, only show the composebox if the
    // forcedcomposeboxBounds are set. No-op if the feature flag is not enabled.
    if (this.enableComposeboxJumpFix_ && this.isAiPage_ && !this.isZeroState_ &&
        !this.forcedComposeboxBounds_) {
      return true;
    }

    // In all other cases, show the composebox.
    return false;
  }

  protected isComposeboxHeaderWrapperHidden_(): boolean {
    return (this.enableBasicMode_ && this.isInBasicMode_ &&
            !this.enableBasicModeZOrder_) ||
        this.inNlm_;
  }

  private checkInNlm_(url: URL): boolean {
    if (!this.enableCustomNlmUi_) {
      return false;
    }
    return url.searchParams.has(this.nlmUrlParam_);
  }

  getComposeboxBoundsStyles() {
    if ((this.isZeroState_ && !this.inNlm_) || !this.forcedComposeboxBounds_) {
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
      `position: fixed;`,
      `bottom: ${window.innerHeight - relativeRect.bottom}px;`,
      `left: ${relativeRect.left}px;`,
      `width: ${relativeRect.width}px;`,
      `margin: 0;`,
      `max-width: none;`,
      `min-width: 0;`,
    ];
    return style.join(' ');
  }

  getThreadFrameStyles(): string {
    if (this.occluders_ == null) {
      return '';
    }

    const composebox = this.composebox_;
    if (!composebox) {
      return '';
    }

    // If the forced composebox bounds are set, use those since its cheaper
    // than calling getBoundingClientRect();
    const composeboxBounds = this.forcedComposeboxBounds_ ??
        this.getComposeboxBoundsRelativeToThreadFrame_();

    const frameRect = this.$.threadFrame.getBoundingClientRect();

    // If occluders are present, set the clip path and a z-index that ensures
    // the thread frame is above the occluders.
    const roundedClipPathEnabled =
        loadTimeData.getBoolean('roundedClipPathEnabled');
    const borderRadius =
        roundedClipPathEnabled ? COMPOSEBOX_BORDER_RADIUS_PX : 0;

    const result =
        getNonOccludedClipPath(
            composeboxBounds, this.occluders_, OCCLUDER_EXTRA_PADDING_PX,
            frameRect.width, frameRect.height, borderRadius) +
        'z-index: 100;';
    return result;
  }

  protected getComposeboxBoundsRelativeToThreadFrame_() {
    const composebox = this.composebox_;
    if (!composebox) {
      return null;
    }
    const frameRect = this.$.threadFrame.getBoundingClientRect();
    const composeboxRect = composebox.getBoundingClientRect();
    return {
      top: composeboxRect.top - frameRect.top,
      left: composeboxRect.left - frameRect.left,
      width: composeboxRect.width,
      height: composeboxRect.height,
      right: composeboxRect.right - frameRect.left,
      bottom: composeboxRect.bottom - frameRect.top,
    };
  }

  protected async onNewThreadClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.OpenNewThread');
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
    const composebox = this.composebox_;
    if (composebox) {
      composebox.startExpandAnimation();
      composebox.clearInputAndFocus();
    }
  }

  get isLoadErrorForTesting() {
    return this.isLoadError_;
  }


  getEnableNativeZeroStateSuggestionsForTesting() {
    return this.enableNativeZeroStateSuggestions_;
  }

  setEnableNativeZeroStateSuggestionsForTesting(enable: boolean) {
    this.enableNativeZeroStateSuggestions_ = enable;
  }

  setIsInBasicModeForTesting(isInBasicMode: boolean) {
    this.isInBasicMode_ = isInBasicMode;
  }

  getForcedComposeboxBoundsForTesting(): Rect|null {
    return this.forcedComposeboxBounds_;
  }

  private postMessageToWebview(message: number[]) {
    assert(this.postMessageHandler_);
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
    assert(this.postMessageHandler_);
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
    this.updateBackgroundColor_();
    this.commonSearchParams_ = params;
    this.maybeLoadPendingUrl_();
  }

  private setupWebviewRequestOverrides() {
    if (isFullWebView(this.$.threadFrame)) {
      this.$.threadFrame.request.onBeforeRequest.addListener(
          this.onBeforeRequest, {
            types: ['main_frame' as chrome.webRequest.ResourceType],
            urls: ['<all_urls>'],
          },
          ['blocking']);

      // Allow downloading files. This is necessary since aim can generate
      // images for download.
      this.$.threadFrame.addEventListener(
          'permissionrequest',
          (e: chrome.webviewTag.PermissionRequestEvent|
           PermissionRequestEvent) => {
            if (e.permission === 'download') {
              e.request.allow();
            }
          });

      // Sets the user agent to the default user agent + the contextual tasks
      // custom suffix.
      const userAgent = this.$.threadFrame.getUserAgent();
      const userAgentSuffix = loadTimeData.getString('userAgentSuffix');
      this.$.threadFrame.setUserAgentOverride(
          `${userAgent} ${userAgentSuffix}`);
    }
  }

  private removeWebviewRequestOverrides() {
    if (isFullWebView(this.$.threadFrame)) {
      this.$.threadFrame.request.onBeforeRequest.removeListener(
          this.onBeforeRequest);
    }
  }

  private addCommonSearchParams(url: URL): URL {
    if (!this.commonSearchParams_) {
      return url;
    }
    for (const [key, value] of Object.entries(this.commonSearchParams_)) {
      // If the url already has a key, skip it to avoid overriding it. `cs` is an
      // exception since it will cause UI mismatch between native and embedded
      // page.
      if (key !== 'cs' && url.searchParams.has(key)) {
        continue;
      }
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

            if (this.host_ && !isSigninDomain) {
              newUrl.host = this.host_;
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

  updateTooltipVisibilityForTesting() {
    this.updateTooltipVisibility_();
  }

  // Onboarding tooltip is not supported on Android.
  // <if expr="not is_android">
  get numberOfTimesTooltipShownForTesting() {
    return this.$.onboardingTooltip?.numberOfTimesTooltipShownForTesting ?? 0;
  }

  set numberOfTimesTooltipShownForTesting(n: number) {
    if (this.$.onboardingTooltip) {
      this.$.onboardingTooltip.numberOfTimesTooltipShownForTesting = n;
    }
  }

  set userDismissedTooltipForTesting(dismissed: boolean) {
    if (this.$.onboardingTooltip) {
      this.$.onboardingTooltip.userDismissedTooltipForTesting = dismissed;
    }
  }

  get tooltipResizeObserverForTesting() {
    return this.$.onboardingTooltip?.tooltipResizeObserverForTesting ?? null;
  }
  // </if>

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

  // Since this is a derivative state, this is one of the only protected
  // or private members that should have testing setter method.
  // The rest should be changed through testProxy routerRemote.
  setIsNavigatingFromAiPageForTesting(isNavigatingForTesting: boolean) {
    this.isNavigatingFromAiPage_ = isNavigatingForTesting;
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

  onThreadFrameLoadStartForTesting(
      event: chrome.webviewTag.LoadStartEvent|LoadEvent) {
    this.onThreadFrameLoadStart(event);
  }

  onThreadFrameLoadCommitForTesting(
      event: chrome.webviewTag.LoadCommitEvent|LoadEvent) {
    this.onThreadFrameLoadCommit(event);
  }

  onThreadFrameContentLoadForTesting() {
    this.onThreadFrameContentLoad();
  }

  async onThreadFrameLoadAbortForTesting(
      event: chrome.webviewTag.LoadAbortEvent|LoadAbortEvent) {
    await this.onThreadFrameLoadAbort(event);
  }

  setIsZeroStateForTesting(isZeroState: boolean|undefined) {
    this.isZeroState_ = isZeroState;
  }

  setInNlmForTesting(inNlm: boolean) {
    this.inNlm_ = inNlm;
  }

  setForcedComposeboxBoundsForTesting(bounds: Rect|null) {
    this.forcedComposeboxBounds_ = bounds;
  }

  getOccludersForTesting(): Rect[]|null {
    return this.occluders_;
  }

  getPendingBasicModeForTesting(): boolean|null {
    return this.pendingBasicMode_;
  }

  private updateBackgroundColor_() {
    if (this.darkMode_) {
      document.body.style.backgroundColor = this.useStratusDarkModeColors_ ?
          'rgba(34, 36, 43, 1)' :
          'rgba(16, 18, 23, 1)';
    } else {
      document.body.style.backgroundColor = 'rgba(255, 255, 255, 1)';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-app': ContextualTasksAppElement;
  }
}

customElements.define(ContextualTasksAppElement.is, ContextualTasksAppElement);
