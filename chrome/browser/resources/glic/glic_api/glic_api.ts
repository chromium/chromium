// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API between the Chrome browser and the Glic web client.
//
// Follow some notes providing more context about the Glic API and guidelines on
// how the web client code should be constructed around it. Check the internal
// documentation at http://shortn/_xFTHEnFhDV for more details.
//
// - There may be multiple instances of the web client running at a time, all
//   sharing the same local web storage space. Whenever one is started or
//   restarted, the initialization steps will be repeated.
// - The defined functions and interfaces can be "evolved" to provide more
//   functionality and data, as needed, but must be kept backwards compatible.
// - Functions are documented with their known behavior. Exceptions and promise
//   failures should be documented only if they are expected.
// - As in TypeScript all `number`s are 64 bit floating points, we decided to
//   make all identifier values be of the `string` type (e.g. for a window or a
//   tab).
// - The browser provided tab and window IDs are based on the browser's
//   SessionID values, which are not stable between Chrome restarts, and should
//   not be saved to persisted storage for later reuse. See:
//   https://crsrc.org/c/components/sessions/core/session_id.h
// - URLs can be arbitrarily long but any URL sent or received through this API
//   will be silently made empty if exceeding the 2 MiB length limit imposed by
//   Mojo's URL implementation. See:
//   https://crsrc.org/c/url/mojom/url.mojom
// - Avoid doing exhaustive checks against enums defined by the API, as their
//   values may evolve over time.

/// BEGIN_GENERATED - DO NOT MODIFY BELOW
import * as generated from './glic_api_generated.js';
export import ActivateTabOptions = generated.ActivateTabOptions;
export import AdditionalContext = generated.AdditionalContext;
export import AnnotatedPageData = generated.AnnotatedPageData;
export import AutofillSuggestion = generated.AutofillSuggestion;
export import CaptureRegionParams = generated.CaptureRegionParams;
export import CaptureRegionResult = generated.CaptureRegionResult;
export import CapturedRegion = generated.CapturedRegion;
export import ConversationInfo = generated.ConversationInfo;
export import CounterAbuseVerdict = generated.CounterAbuseVerdict;
export import CreateActorTabOptions = generated.CreateActorTabOptions;
export import CreateSkillRequest = generated.CreateSkillRequest;
export import CreateTabOptions = generated.CreateTabOptions;
export import Credential = generated.Credential;
export import DocumentData = generated.DocumentData;
export import ExperimentalTriggeringUpdate =
    generated.ExperimentalTriggeringUpdate;
export import FormFillingRequest = generated.FormFillingRequest;
export import FormFillingResponse = generated.FormFillingResponse;
export import FrameMetadata = generated.FrameMetadata;
export import GeminiEnterpriseSettings = generated.GeminiEnterpriseSettings;
export import GetPinCandidatesOptions = generated.GetPinCandidatesOptions;
export import ImageBytesResult = generated.ImageBytesResult;
export import ImageInfo = generated.ImageInfo;
export import InvokeOptions = generated.InvokeOptions;
export import MetaTag = generated.MetaTag;
export import OnResponseStoppedDetails = generated.OnResponseStoppedDetails;
export import OpenSettingsOptions = generated.OpenSettingsOptions;
export import PageMetadata = generated.PageMetadata;
export import PanelOpeningData = generated.PanelOpeningData;
export import PanelState = generated.PanelState;
export import ParentConversationMetadata = generated.ParentConversationMetadata;
export import PdfDocumentData = generated.PdfDocumentData;
export import PendingCapturedRegion = generated.PendingCapturedRegion;
export import PinCandidate = generated.PinCandidate;
export import PinTabsOptions = generated.PinTabsOptions;
export import SafeBrowsingVerdict = generated.SafeBrowsingVerdict;
export import Screenshot = generated.Screenshot;
export import ScreenshotCollectionOptions =
    generated.ScreenshotCollectionOptions;
export import ScrollToNodeSelector = generated.ScrollToNodeSelector;
export import ScrollToParams = generated.ScrollToParams;
export import ScrollToSelector = generated.ScrollToSelector;
export import ScrollToTextFragmentSelector =
    generated.ScrollToTextFragmentSelector;
export import ScrollToTextSelector = generated.ScrollToTextSelector;
export import SelectAutofillSuggestionsDialogRequest =
    generated.SelectAutofillSuggestionsDialogRequest;
export import SelectCredentialDialogRequest =
    generated.SelectCredentialDialogRequest;
export import SelectCredentialDialogResponse =
    generated.SelectCredentialDialogResponse;
export import Skill = generated.Skill;
export import SkillPreview = generated.SkillPreview;
export import SuggestionContent = generated.SuggestionContent;
export import TabContextOptions = generated.TabContextOptions;
export import TabContextResult = generated.TabContextResult;
export import TabData = generated.TabData;
export import TaskOptions = generated.TaskOptions;
export import UniversalCartPayload = generated.UniversalCartPayload;
export import UnpinTabsOptions = generated.UnpinTabsOptions;
export import UpdateSkillRequest = generated.UpdateSkillRequest;
export import UserProfileInfo = generated.UserProfileInfo;
export import WebPageData = generated.WebPageData;
export import ZeroStateSuggestions = generated.ZeroStateSuggestions;
export import ZeroStateSuggestionsOptions =
    generated.ZeroStateSuggestionsOptions;
export import ZeroStateSuggestionsV2 = generated.ZeroStateSuggestionsV2;
export import ZssConfig = generated.ZssConfig;
export import ActorTaskInterruptReason = generated.ActorTaskInterruptReason;
export import ActorTaskPauseReason = generated.ActorTaskPauseReason;
export import ActorTaskState = generated.ActorTaskState;
export import ActorTaskStopReason = generated.ActorTaskStopReason;
export import ActuationTarget = generated.ActuationTarget;
export import AdditionalContextSource = generated.AdditionalContextSource;
export import CancelActionsResult = generated.CancelActionsResult;
export import CaptureRegionErrorReason = generated.CaptureRegionErrorReason;
export import CaptureScreenshotErrorReason =
    generated.CaptureScreenshotErrorReason;
export import ClientCapabilities = generated.ClientCapabilities;
export import ClientErrorDialogType = generated.ClientErrorDialogType;
export import CreateTaskErrorReason = generated.CreateTaskErrorReason;
export import CredentialType = generated.CredentialType;
export import ExperimentalTriggeringUpdateType =
    generated.ExperimentalTriggeringUpdateType;
export import FeatureMode = generated.FeatureMode;
export import FormFactor = generated.FormFactor;
export import FreOverride = generated.FreOverride;
export import HostCapability = generated.HostCapability;
export import InvocationSource = generated.InvocationSource;
export import LightweightPageFeature = generated.LightweightPageFeature;
export import MetricUserInputReactionType =
    generated.MetricUserInputReactionType;
export import MicrophoneStatus = generated.MicrophoneStatus;
export import PanelStateKind = generated.PanelStateKind;
export import PerformActionsErrorReason = generated.PerformActionsErrorReason;
export import PinTrigger = generated.PinTrigger;
export import Platform = generated.Platform;
export import RegisterConversationErrorReason =
    generated.RegisterConversationErrorReason;
export import SbThreatType = generated.SbThreatType;
export import ScreenshotCompressionQuality =
    generated.ScreenshotCompressionQuality;
export import ScreenshotEncryptionScheme = generated.ScreenshotEncryptionScheme;
export import ScreenshotImageFormat = generated.ScreenshotImageFormat;
export import ScrollToErrorReason = generated.ScrollToErrorReason;
export import SettingsPageField = generated.SettingsPageField;
export import SkillSource = generated.SkillSource;
export import SkillsWebClientEvent = generated.SkillsWebClientEvent;
export import SwitchConversationErrorReason =
    generated.SwitchConversationErrorReason;
export import TaskDuration = generated.TaskDuration;
export import UnpinTrigger = generated.UnpinTrigger;
export import UserGrantedPermissionDuration =
    generated.UserGrantedPermissionDuration;
export import WebClientMode = generated.WebClientMode;
export import WebClientModel = generated.WebClientModel;
export import WebUseCounter = generated.WebUseCounter;

/// END_GENERATED - DO NOT MODIFY ABOVE

/** Allows the Glic web client to register with the host WebUI. */
export declare interface GlicHostRegistry {
  /**
   * Registers the GlicWebClient instance with the browser. On success, the
   * browser will follow up with a call to `GlicWebClient#initialize`. A
   * rejection of the promise indicates a browser side failure.
   *
   * The web client must call this once when its webview on-load event is fired.
   *
   * This should only be called once! Subsequent calls will break.
   */
  registerWebClient(webClient: GlicWebClient): Promise<void>;
}

/** Part of an additional context object. Only one field will be present. */
export declare interface AdditionalContextPart {
  /**
   * The context data. The MIME type is available from the `type` property.
   * Callers can use `arrayBuffer()` to get the data as a buffer, or `stream()`
   * to read it as a stream if the data is large.
   */
  data?: Blob;
  /** The filename of the data, if available. */
  filename?: string;
  /**
   * The following four fields can be contained by `tabContext` and are
   * deprecated
   */
  screenshot?: Screenshot;
  webPageData?: WebPageData;
  annotatedPageData?: AnnotatedPageData;
  pdf?: PdfDocumentData;
  tabContext?: TabContextResult;
  region?: CapturedRegion;
  pendingRegion?: PendingCapturedRegion;
  parentConversationMetadata?: ParentConversationMetadata;
}

/** Union representing source-specific payloads. */
export declare interface InvocationPayload {
  universalCart?: UniversalCartPayload;
}

/**
 * Data structure sent from the browser to the web client with panel opening
 * information.
 */

/**
 * Implemented by the Glic web client, with its methods being called by the
 * browser. Most functions are optional.
 */
export declare interface GlicWebClient {
  /**
   * Signals to the web client that it should initialize itself and get ready to
   * be invoked by the user. The provided GlicBrowserHost instance offers
   * information access functions so that the web client can configure itself
   * That instance should also be stored for later usage.
   *
   * Successful initialization should be signaled with a void promise return, in
   * which case the web client is considered ready to be invoked by the user and
   * initialize() will not be called again.
   *
   * A failed promise means initialization failed and it will not be retried.
   * @throws {WebClientInitializeError}
   */
  initialize(glicBrowserHost: GlicBrowserHost): Promise<void>;

  /**
   * @todo The browser is currently storing the previous panel size, but the web
   *       client should be updated to set the panel size when handling this
   *       call. https://crbug.com/392141194
   *
   * @todo Remove PanelState from the input argument type once the web client
   *       adopts PanelOpeningData.
   *       https://crbug.com/402147705
   *
   * Called right before the panel is made visible to the user. This event is
   * always called no matter how the panel opening is initiated.
   *
   * The web client should use the handling of this call to execute any
   * preparations needed to become user-visible, and return a fully populated
   * OpenPanelInfo. `panelOpeningData` holds information about the open request
   * and the state of the panel that is about to be presented.
   *
   * Important: The panel is only made user-visible once the returned promise is
   * resolved or failed (failures are ignored and the panel is still shown).
   *
   * WARNING: Chrome may call this multiple times over the lifetime of the
   * panel, even while the panel is already open. These calls may indicate that
   * the panel was opened on a different tab's side panel or as a floating
   * window. The web client should still inspect and react to the
   * `panelOpeningData` in these cases.
   */
  notifyPanelWillOpen?
    (panelOpeningData: PanelOpeningData & PanelState): Promise<OpenPanelInfo>;

  /**
   * Called right after the panel was hidden away and is not visible to
   * the user anymore. This event is always called no matter how the panel
   * closing is initiated.
   *
   * Important: The browser will keep the web client alive at least until the
   * promise is resolved or failed (failures are ignored).  After that, the
   * browser will not necessarily terminate the web client right away, but may
   * do so at any time.
   */
  notifyPanelWasClosed?(): Promise<void>;

  /**
   * The web client should resolve the promise after verifying the app is
   * responsive.
   *
   * If the host receives no response within 500 ms, it will flag the web client
   * as unresponsive and displaying an error state to the user.
   */
  checkResponsive?(): Promise<void>;

  /**
   * Invokes Glic with specific options.
   * This can be called to open the panel or update an existing session.
   * Returns when the invocation has been received and processed by the client.
   * @throws {Error} on failure.
   */
  invoke?(options: InvokeOptions): Promise<void>;

  /**
   * Requests the web client to stop microphone recording.
   */
  stopMicrophone?(): Promise<void>;

  /**
   * Returns the list of capabilities of the glic web client.
   * This will be called by Chrome once, prior to initialize(),
   * and the result will be cached for the lifetime of the web client.
   */
  getClientCapabilities?(): Set<ClientCapabilities>;

  /*
   * Returns an observable that emits all the glic updates for this instance.
   * A terminal `ExperimentalTriggeringUpdateType` in the
   * `ExperimentalTriggeringUpdate` payload should be followed by the completion
   * of the observable to ensure proper unsubscribing and cleanup operations
   * take place.
   */
  getExperimentalTriggeringUpdates?
    (): Observable2<ExperimentalTriggeringUpdate>;

  // !!! ATTENTION !!!
  // Avoid adding new methods to this interface! Instead, to push information to
  // the web client it's much more preferable to add new functions to
  // GlicBrowserHost that return an Observable or ObservableValue instances.
}

/** Request object for uploading an encrypted screenshot of the active tab. */
export declare interface ExperimentalTriggeringUploadScreenshotRequest {
  /** The screenshot to upload. */
  screenshot: Screenshot;
  /** Client must call when upload has finished. */
  uploadComplete(fileToken: string|null): void;
}

/**
 * Interface providing experimental triggering capabilities from the browser
 * host.
 */
export declare interface GlicExperimentalTriggeringBrowserHost {
  /**
   * Returns an Observable emitting requests when the browser triggers an
   * encrypted screenshot capture and upload.
   */
  uploadEncryptedScreenshotRequests?
      (): Observable<ExperimentalTriggeringUploadScreenshotRequest>;
}

/**
 * Provides functionality implemented by the browser to the Glic web client.
 * Most functions are optional.
 */
export declare interface GlicBrowserHost {
  /** Returns the precise Chrome's version. */
  getChromeVersion(): Promise<ChromeVersion>;

  /**
   * Returns the experimental triggering host interface, or undefined if
   * experimental triggering is disabled.
   */
  experimentalTriggering?(): GlicExperimentalTriggeringBrowserHost;

  /** Return the platform glic is running on. */
  getPlatform?(): Platform;

  /** Return the form factor of the device glic is running on. */
  getFormFactor?(): FormFactor;

  /**
   * Notifies the browser that the web client has switched modes. Note that this
   * call does not change any aspect of the panel itself (e.g. resize-ability).
   *
   * This should be called by the web client whenever it switches modes. It
   * should not be called, though, when the panel is being opened, as the
   * opening mode is already part of the information returned to the browser by
   * `notifyPanelWillOpen`.
   *
   * @param newMode the mode the web client switched into.
   */
  onModeChange?(newMode: WebClientMode): void;

  /**
   * Sets the size of the glic window to the specified dimensions. Resolves when
   * the animation finishes, is interrupted, or immediately if the window
   * doesn't exist yet, in which case the size will be used as the initial size
   * when the widget is eventually created. Size values are in DIPs.
   *
   * All provided values will go through sanity checks (e.g. checking min and
   * max values for height and width) and may be adjusted. The web client should
   * expect that the provided values may not be applied verbatim.
   */
  resizeWindow(width: number, height: number, options?: ResizeWindowOptions):
    Promise<void>;

  /**
   * Set the state of the panel's user drag-to-resize capability, or if the
   * panel hasn't been created yet, set whether it will be user resizable when
   * it is created.
   */
  enableDragResize?(enabled: boolean): Promise<void>;

  /**
   * Sets the minimum possible size a user can resize to for the glic window.
   *
   * All provided values will go through sanity checks (e.g. checking min
   * values for height and width) and may be adjusted. The web client should
   * expect that the provided values may not be applied verbatim. Note: This
   * will not affect the current glic window size.
   */
  setMinimumWidgetSize?(width: number, height: number): Promise<void>;

  /**
   * Returns the model quality client ID.
   *
   * IMPORTANT: callers must verify that getHostCapabilities() includes
   * HostCapability.GET_MODEL_QUALITY_CLIENT_ID before calling this API.
   * Checking that it's defined is not sufficient. In older Chromium versions
   * this method can be unsafe to call even when it's defined.
   */
  getModelQualityClientId?(): Promise<string>;

  /**
   * Returns the Gemini Enterprise settings if available.
   * New in May 2026.
   */
  getGeminiEnterpriseSettings?
      (): ObservableValue<GeminiEnterpriseSettings|undefined>;

  /**
   * Fetches page context for the currently focused tab, optionally including
   * more expensive-to-generate data. Requesting only the base data is cheap,
   * but the returned information should be identical to the latest push-update
   * received by the web client from `getFocusedTabStateV2`.
   *
   * All optional data, which are expensive to extract, should only be requested
   * when necessary.
   *
   * Critically, this function may return information from a previously focused
   * page due its asynchronous nature. To confirm, tabId and URL should match
   * the respective values of the tab of interest.
   *
   * @throws {Error} on failure.
   */
  getContextFromFocusedTab?
    (options: TabContextOptions): Promise<TabContextResult>;

  /**
   * Similar to `getContextFromFocusedTab`, but returns context from the tab
   * identified by `tabId`. Will fail if the tab is not pinned or focused.
   *
   * @throws {Error} on failure.
   */
  getContextFromTab?
    (tabId: string, options: TabContextOptions): Promise<TabContextResult>;

  /**
   * Similar to `getContextFromTab`, but for actors. Skips the focus check.
   *
   * @throws {Error} on failure.
   */
  getContextForActorFromTab?
    (tabId: string, options: TabContextOptions): Promise<TabContextResult>;

  /**
   * Retrieves raw image bytes, MIME type, and metadata for an image node from
   * the tab associated with `tabId`.
   *
   * @throws {Error} on failure.
   */
  getImageBytesFromTab?(tabId: string, documentId: string, domNodeId: number):
      Promise<ImageBytesResult>;

  /**
   * Sets the maximum number of supported pinned tabs. Should not be called
   * more than once. Chrome may not be able to support the given number, so
   * the applied limit is returned.
   */
  setMaximumNumberOfPinnedTabs?(numTabs: number): Promise<number>;

  /**
   * @deprecated Use CreateTask and PerformActions instead. This method
   * is undefined in Chrome and calling it is not supported.
   *
   * Inform Chrome about an action. Chrome Takes an action based on the
   * action proto and returns new context based on the tab context options.
   *
   * Attempts to act while the associated task is stopped/paused will be
   * rejected.
   *
   * @throws {ActInFocusedTabError} on failure.
   */
  actInFocusedTab?
    (params: ActInFocusedTabParams): Promise<ActInFocusedTabResult>;

  /**
   * Creates a task and returns its ID. The optional @param taskOptions
   * contains information about the task that is being created.
   *
   * @throws {ActInFocusedTabError} on failure.
   *
   */
  createTask?(taskOptions?: TaskOptions): Promise<number>;

  /**
   * Performs actions on the task with the given ID.
   *
   * The input corresponds to the Actions proto in
   * components/optimization_guide/proto/features/actions_data.proto.
   *
   * The output corresponds to the ActionsResult proto.
   *
   */
  performActions?(actions: ArrayBuffer): Promise<ArrayBuffer>;

  /**
   * Cancel the actions for the specified actor task. It does not revert actions
   * already taken. Returns an error if the task is not found.
   *
   * @param taskId - The ID of the target actor task.
   * @returns A promise resolving to a {@link CancelActionsResult}
   *     indicating the outcome.
   */
  cancelActions?(taskId: number): Promise<CancelActionsResult>;

  /**
   * Stops the actor task with the given ID in the browser if it exists. No-op
   * otherwise.
   *
   * Stopping a task removes all actor related restrictions from the associated
   * tab. Any in progress actions are canceled and the associated Promises are
   * rejected.
   *
   * If the stopReason is not provided, it uses the default value
   * ActorTaskStopReason.TASK_COMPLETE.
   *
   */
  stopActorTask?(taskId?: number, stopReason?: ActorTaskStopReason): void;

  /**
   * Pauses the actor task with the given ID in the browser if it exists. No-op
   * otherwise.
   *
   * Pausing a task removes actor related restrictions that prevent the user
   * from interacting with the associated tab. Any in progress actions are
   * canceled and the associated Promises are rejected.
   *
   * If the tabId is provided, it is added to the actor task.
   *
   * If the pauseReason is not provided, it uses the default value
   * ActorTaskPauseReason.PAUSED_BY_MODEL.
   *
   */
  pauseActorTask?
    (taskId: number, pauseReason?: ActorTaskPauseReason, tabId?: string):
    void;

  /**
   * Resumes a previously paused actor task with the given ID.
   *
   * Returns the tab context at the time of resumption, based on the provided
   * context options.
   *
   * @throws {Error} on failure.
   *
   */
  resumeActorTask?(taskId: number, tabContextOptions: TabContextOptions):
    Promise<ResumeActorTaskResult>;

  /**
   * Interrupts the actor task with the given ID in the browser if it exists.
   * No-op otherwise.
   *
   * Interrupting is different than pausing. Interrupting changes the state
   * indicating the task is waiting for user input but does not pause the
   * task.
   *
   * @param interruptReason The reason for why the interrupt was initiated.
   */
  interruptActorTask?
    (taskId: number, interruptReason?: ActorTaskInterruptReason): void;

  /**
   * Indicates a task is no longer interrupted with the given ID in the browser
   * if it exists. No-op otherwise.
   */
  uninterruptActorTask?(taskId: number): void;

  /**
   * Returns the observable state of the actor task with the given ID. Updates
   * are sent whenever:
   * - The task is created, paused, resumed or stopped.
   * - The task is performing an action.
   * - The task is going away.
   */
  getActorTaskState?(taskId: number): ObservableValue<ActorTaskState>;

  /**
   * Creates a new tab for acting, using the initiator tab and window to
   * determine the window the tab will be created in. Returns the TabData for
   * the newly created tab (which may be empty in case of failure).
   *
   * taskId: Is the actor task id associated with this request. Note: this is
   * used only to associate this call with a task in the journal; the new tab
   * isn't associated with the task until an action is performed on the tab.
   */
  createActorTab?(taskId: number, createActorTabOptions: CreateActorTabOptions):
    Promise<TabData>;

  /**
   * Returns the observable state of TabData for the given tab.
   *
   * The returned observable is completed when the tab is destroyed, or one is
   * not found with the given ID.
   */
  getTabById?(tabId: string): ObservableValue<TabData>;

  /**
   * Returns an observable of the favicon for the given tab.
   *
   * New in March 2026. This will replace favicon access from TabData.
   * This is the only favicon access that works on Android.
   *
   * The returned observable is completed when the tab is destroyed, or one is
   * not found with the given ID. If the tab has no favicon, the observable
   * will emit undefined.
   */
  getTabFaviconById?(tabId: string): ObservableValue<Blob | undefined>;

  /**
   * Makes the given tab the active tab in its window and activates its window.
   *
   * No-op if the tab doesn't exist or is already in the foreground.
   */
  activateTab?(tabId: string): void;

  /**
   * Requests the host to capture a screenshot. The choice of the screenshot
   * target is made by the host, possibly allowing the user to choose between a
   * desktop, window or arbitrary region.
   *
   * The promise will be failed if the user rejects the capture or another
   * problem happens.
   *
   * @throws {CaptureScreenshotError} on failure.
   */
  captureScreenshot?(): Promise<Screenshot>;

  /**
   * Starts a user-interactive process to select content from a tab. The user
   * can select multiple regions.
   *
   * Uses the optional `params` to control the capture behavior and target tab.
   *
   * The returned observable will emit a value for each region captured. The
   * client can cancel this operation by unsubscribing from the observable,
   * which will cause the observable to be completed.
   *
   * The observable will terminate with a `CaptureRegionError` if the operation
   * fails. This can happen if there is no focusable tab to capture from (with
   * reason `NO_FOCUSABLE_TAB`), or if the operation is canceled by the user or
   * the browser for other reasons (with reason `UNKNOWN`).
   *
   * Only one capture operation can be active at a time across all instances of
   * the Glic web client running within a single Chrome user profile.
   *
   * If a capture is already in progress when this method is called (either from
   * the same or a different client instance), the existing capture session will
   * be terminated and a new one will begin.
   */
  captureRegion?
    (params?: CaptureRegionParams): ObservableValue<CaptureRegionResult>;

  /**
   * Deletes a captured region.
   *
   * @param tabId The ID of the tab from which the region was captured.
   * @param id The ID of the captured region to delete.
   */
  deleteCapturedRegion?(tabId: string, id: string): void;

  /**
   * @todo All actuation should eventually be moved onto PerformActions.
   *
   * Creates a tab and navigates to a URL. It is made the active tab by default
   * but that can be changed using `options.openInBackground`.
   *
   * Only HTTP and HTTPS schemes are accepted.
   *
   * The tab is created in the currently active window by default. If
   * `options.windowId` is specified, it is created within the respective
   * window.
   *
   * The promise returns information about the newly created tab. The promise
   * may be rejected in case of errors that prevented the tab from being
   * created. An invalid scheme or `options.windowId` will cause a promise
   * failure.
   *
   * Note: This function does not return loading information for the newly
   * created tab. If that's needed, we can add another function that does it.
   */
  createTab?(url: string, options: CreateTabOptions): Promise<TabData>;

  /**
   * Activates an existing tab matching the exact url or the wildcard pattern in
   * options (if provided) across browser windows, or creates a new tab if no
   * matching tab is found.
   */
  activateTabWithUrl?
      (exactUrl: string, options?: ActivateTabOptions): Promise<TabData>;

  /**
   * Opens a tab with the glic settings page, optionally highlighting a specific
   * field in it. If an open tab already has the glic settings page loaded, it
   * is focused instead.
   */
  openGlicSettingsPage?(options?: OpenSettingsOptions): void;

  /**
   * Opens a tab to the password manager settings page. If an open tab already
   * has the page loaded, it is focused instead.
   */
  openPasswordManagerSettingsPage?(): void;

  /** Requests the closing of the panel containing the web client. */
  closePanel?(): Promise<void>;

  /**
   * Similar to closePanel but also requests that the web client be torn down.
   * Normally, Chrome manages creation and destruction of the web client. This
   * function is a fallback solution to permit the web client to limit its
   * lifetime, if needed.
   */
  closePanelAndShutdown?(): void;

  /**
   * Requests that the web client's panel be attached to a browser window.
   * If attachment fails, the panel's state will not be updated. getPanelState
   * can be used to monitor whether attachment is successful.
   */
  attachPanel?(): void;

  /**
   * Requests that the web client's panel be detached from a browser window
   * (floats free).
   *
   * @throws {Error} If NO_LIVE_MODE is enabled.
   */
  detachPanel?(): void;

  /**
   * Triggers the change profile flow, which allows the user to switch which
   * profile is used. If a new profile is chosen, this web client will be
   * closed in favor of a new one.
   */
  showProfilePicker?(): void;

  /**
   * Returns the state of the panel.
   */
  getPanelState?(): ObservableValue<PanelState>;

  /**
   * Whether the glic panel is in the active state. In the inactive state,
   * microphone recording should stop, but any playing audio output can
   * continue.
   *
   * For these purposes, a panel is considered active if it is open, even
   * if the window containing the panel is not active.
   */
  panelActive(): ObservableValue<boolean>;

  /**
   * Whether the panel can be attached. This is true if there is a browser
   * window suitable for attachment. This state is only meaningful when the
   * panel is in the detached state, and should be not be considered otherwise
   * (i.e. it will not necessarily become false when the panel becomes
   * attached). When this is false, calls to attachPanel() are likely to fail.
   */
  canAttachPanel?(): ObservableValue<boolean>;

  /**
   * Whether any browser windows are open for this profile. This exists to allow
   * the web client to signal the user that they need to open a browser window
   * before sharing context. A browser window being open does not guarantee
   * there is a focused tab or a panel can attach to the browser.
   */
  isBrowserOpen?(): ObservableValue<boolean>;

  /**
   * Returns the observable state of the currently focused tab. Updates are sent
   * whenever:
   * - The user switches active tabs, which causes a change in `tabId`.
   * - The tab navigates to a new page, which causes a change in `url`.
   * - The user moves the current tab to a new window,  which causes a change in
   *   `windowId`.
   * - The user switches active windows, which would definitely change both
   *   `tabId` and `windowId` and, likely, all other data fields, too.
   * - The user switches between tabs that can and cannot be focused (or
   *   vice-versa), which changes which field has a value set between `hasFocus`
   *   and `hasNoFocus`.
   * - Any other data represented in `TabData` (title, favicon, mime type)
   *   changes and needs to be updated to match the respective tab state.
   *   Updates are possible throughout the lifetime of a page.
   */
  getFocusedTabStateV2?(): ObservableValue<FocusedTabData>;

  /** Returns the state of the microphone permission. */
  getMicrophonePermissionState?(): ObservableValue<boolean>;

  /** Returns the state of the location permission. */
  getLocationPermissionState?(): ObservableValue<boolean>;

  /**
   * Returns the state of the tab context permission for this instance.
   *
   * Note: This state may differ from the global default if per-instance
   * permissions are enabled and the user has toggled access for this specific
   * instance.
   */
  getTabContextPermissionState?(): ObservableValue<boolean>;

  /** Returns the state of the OS granted location permission. */
  getOsLocationPermissionState?(): ObservableValue<boolean>;

  /** Returns the state of the OS hotkey. */
  getOsHotkeyState?(): ObservableValue<{ hotkey: string }>;

  /** Returns the state of the glic closed captioning setting. */
  getClosedCaptioningSetting?(): ObservableValue<boolean>;

  /**
   * Returns the state of the web actuation setting. This reflects a
   * user-controlled toggle for whether actuation is allowed.
   */
  getActuationOnWebSetting?(): ObservableValue<boolean>;

  /**
   * Returns the state of the global default tab context permission set in
   * Chrome settings. New instances inherit this value upon creation. The
   * returned observable will be updated when the global setting changes.
   */
  getDefaultTabContextPermissionState?(): ObservableValue<boolean>;

  /**
   * Returns the zoom level of the Glic webview.
   * The client should subscribe to this to be notified of zoom level changes.
   * The value is a float representing the zoom factor (e.g., 1.5 for 150%).
   */
  getZoomLevel?(): ObservableValue<number>;

  /**
   * Set the state of the microphone permission in settings. Returns a promise
   * that resolves when the browser has stored the new pref value.
   */
  setMicrophonePermissionState(enabled: boolean): Promise<void>;

  /**
   * Set the state of the location permission in settings. Returns a promise
   * that resolves when the browser has stored the new pref value.
   */
  setLocationPermissionState(enabled: boolean): Promise<void>;

  /**
   * Set the state of the tab context permission. Returns a promise that
   * resolves when the browser has stored the new value.
   *
   * Note: If per-instance permissions are enabled, this may only update the
   * client's local state optimistically and resolve immediately without
   * modifying a global browser preference.
   */
  setTabContextPermissionState(enabled: boolean): Promise<void>;

  /**
   * Set the state of the closed captioning permission in settings. Returns a
   * promise that resolves when the browser has stored the new pref value.
   */
  setClosedCaptioningSetting?(enabled: boolean): Promise<void>;

  /**
   * Set the state of the web actuation permission in settings. Returns a
   * promise that resolves when the browser has stored the new pref value.
   */
  setActuationOnWebSetting?(enabled: boolean): Promise<void>;

  /** Returns the user profile information. */
  getUserProfileInfo?(): Promise<UserProfileInfo>;

  /**
   * Update Google sign-in cookies for this client. Resolves after the cookies
   * are successfully updated. Rejects if sign-in cookies cannot be updated.
   * This should only be called if the web client detects that it is not
   * signed-in, as Chrome will attempt to refresh cookies automatically in some
   * circumstances. If this is called while a cookie refresh is already in
   * progress, only one cookie refresh will take place.
   */
  refreshSignInCookies?(): Promise<void>;

  /**
   * Show or hide the context access indicators on the focused tab.
   *
   * The indicators visually signal to the user that the client may request
   * context from the currently focused tab. The indicators are only displayed
   * if both the client has enabled them and the host determines that the
   * focused tab is valid for context extraction.
   *
   * The visual effects persist across tab switches and navigations as long as
   * the focused tab remains valid and the client has not hidden the indicators.
   * If the user navigates to an invalid page, the host disables the indicators
   * until navigation back to valid content. The client is responsible for
   * hiding the indicators when tab context permissions are revoked.
   */
  setContextAccessIndicator?(show: boolean): void;

  /**
   * Allows the web client to control audio ducking (aka audio suppression) of
   * other sounds, beyond what's played by itself. It is important that ducking
   * be enabled only when it's necessary (e.g. only during active audio mode).
   */
  setAudioDucking?(enabled: boolean): void;

  /** Returns an object that holds journal-related functionality. */
  getJournalHost?(): GlicBrowserHostJournal;

  /** Returns an object that holds metrics-related functionality. */
  getMetrics?(): GlicBrowserHostMetrics;

  /**
   * @todo Not yet implemented for PDFs. https://crbug.com/395859365
   *
   * Scrolls to and (optionally) highlights content specified by an input
   * selector. Only one highlight is active at a time. Returns a promise that
   * resolves when the selected content is matched and a scroll is started. Only
   * available when `GlicScrollTo` is enabled.
   *
   * @throws {ScrollToError} on failure.
   */
  scrollTo?(params: ScrollToParams): Promise<void>;

  /**
   * Drops the content highlight from scrollTo(). No effects if no contents are
   * highlighted. Only available when `GlicScrollTo` is enabled.
   */
  dropScrollToHighlight?(): void;

  /**
   * Enrolls the Chrome client in the synthetic experiment group specified by
   * trial_name.group_name. Enrollment will only start when the API is called
   * and end when Chrome closes.
   */
  setSyntheticExperimentState?(trialName: string, groupName: string): void;

  /**
   * Opens the OS permission settings page for the given permission type.
   * Supports `media` for microphone and `geolocation` for location.
   * @throws {Error} if the permission type is not supported.
   */
  openOsPermissionSettingsMenu?(permission: OsPermissionType): void;

  /**
   * Get the status of the OS Microphone permission currently granted to Chrome.
   */
  getOsMicrophonePermissionStatus?(): Promise<boolean>;

  /**
   * Returns an observable that signals true when the user starts resizing the
   * panel and false when the user stops.
   */
  isManuallyResizing?(): ObservableValue<boolean>;

  /**
   * Returns the set of zero state suggestions for the currently focused tab
   * based on if the client is currently in it's is_first_run.
   * Callers should verify the current focused tab matches the
   * ZeroStateSuggestions tabId and url before using it.
   */
  getZeroStateSuggestionsForFocusedTab?
    (is_first_run?: boolean): Promise<ZeroStateSuggestions>;

  /**
   * Called when the client believes that the user's status may have changed.
   * For example, an RPC may have been rejected due to the the service being
   * disabled.
   */
  maybeRefreshUserStatus?(): void;

  /**
   * Attempts to pin the given tabs. Can fail if any of the tabs cannot be
   * found, if the number of pinned tabs exceeds the allowed limit or if the tab
   * is already pinned. Return value is true if all tabs were pinned, but if
   * a false value does not mean that no tabs were pinned. The updated set of
   * pinned tabs will asynchronously be available via getPinnedTabs.
   *
   * @param options Options for pinning tabs.
   */
  pinTabs?(tabIds: string[], options?: PinTabsOptions): Promise<boolean>;

  /**
   * Attempts to unpin the given tabs. Can fail if the any of the tabs cannot be
   * found, or if the tab isn't pinned. Return value is true if all tabs were
   * unpinned. A false value does not mean that no tabs were unpinned. The
   * updated set of pinned tabs will asynchronously be available via
   * getPinnedTabs.
   */
  unpinTabs?(tabIds: string[], options?: UnpinTabsOptions): Promise<boolean>;

  /**
   * Unpins all currently pinned tabs.
   */
  unpinAllTabs?(options?: UnpinTabsOptions): void;

  /**
   * Gets TabData for the current set of pinned tabs. The focused tab may also
   * be pinned. That is getFocusedTabStateV2 could have a focused tab that is
   * also in the set of focused tabs. Also fires when TabData for a pinned tab
   * is updated (eg, due to a change of favicon, title, URL, or observability).
   * There is a delay between pinning and unpinning and updates to the set of
   * pinned tabs that will be vended by this API. Callers should not expect that
   * this will be synchronously reflected since this will require a round trip
   * to chrome in order to attempt to pin.
   */
  getPinnedTabs?(): ObservableValue<TabData[]>;

  /**
   * Returns an observable that emits a ranked list of pin tab candidates per
   * the given options. The list is returned once, and then again whenever the
   * list of candidates changes. The results are sorted by string match and then
   * last active time.
   *
   * If a query is provided, it currently returns all top sorted results, even
   * if entries don't match the query.
   *
   * Calling this function will invalidate any previously returned
   * `ObservableValue` instances. So if a previous one existed, it will stop
   * receiving updates when a new one is obtained.
   *
   * Dynamic updates can be a costly operation so the observable should be
   * subscribed only while it is required.
   */
  getPinCandidates?
    (options: GetPinCandidatesOptions): ObservableValue<PinCandidate[]>;

  /**
   * Returns an observable unique to the supplied options that emits zero state
   * suggestions for the currently shared context. The observer will continue
   * to emit subsequent zero state suggestions until it has no more
   * subscribers. Chrome will only maintain one zero state suggestion observer,
   * so calling this again with different options will also cause the previous
   * observer to stop emitting.
   */
  getZeroStateSuggestions?(options?: ZeroStateSuggestionsOptions):
    ObservableValue<ZeroStateSuggestionsV2>;

  /**
   * Creates a skill. The request contains a prompt or an empty string.
   * A Chrome modal will be shown to allow the user to edit and save a skill.
   * The promise will fail if the modal is not opened.
   */
  createSkill?(request: CreateSkillRequest): Promise<void>;

  /**
   * Updates a skill. The request only contains a skill id.
   * The Chrome modal will display the corresponding skill and allow the user to
   * edit and save it. The promise will fail if the modal is not opened.
   */
  updateSkill?(request: UpdateSkillRequest): Promise<void>;

  /**
   * Requests that the browser open skill management UI.
   */
  showManageSkillsUi?(): void;

  /**
   * Requests that the browser open skill browsing UI.
   */
  showBrowseSkillsUi?(): void;

  /**
   * Logs metrics for UI interactions and state transitions specific to the
   * Skills feature in the web client.
   */
  recordSkillsWebClientEvent?(event: SkillsWebClientEvent): void;

  /**
   * Gets a skill by id. The web client should use this method to get the
   * full skill details including the prompt for display or run in the UI.
   * The promise will fail if the skill is not found.
   */
  getSkill?(id: string): Promise<Skill>;

  /**
   * Returns an observable list of skills, which include both 1P and
   * user-created skills. Chrome will update the list when a skill is
   * mutated. Chrome Sync can update multiple skills at once. The web client
   * should use this method to display the full list of skill previews in the
   * "/" menu.
   */
  getSkillPreviews?(): ObservableValue<SkillPreview[]>;

  /**
   * Returns an observable skill to invoke. This happens when user chooses
   * a skill to run in the chrome://skills page. The web client should
   * automatically run the skill when it is received.
   */
  getSkillToInvoke?(): ObservableValue<Skill>;

  /**
   * Returns the list of capabilities of the glic host. The returned set of
   * capabilities will not change during the lifetime of the web client.
   */
  getHostCapabilities?(): Set<HostCapability>;

  /**
   * Returns an observable that emits when PageMetadata for the given tab
   * changes. Only meta tags which are direct children of the head element and
   * match one of the names provided in the names parameter will be
   * monitored.
   *
   * If the tabId is invalid, the observable will complete immediately and not
   * emit, even if the tabId becomes valid later.
   *
   * @throws {Error} if the names parameter is empty.
   *
   * Only one observable per tabId is supported. If a second observable is
   * requested for the same tabId, the first observable will be returned, and
   * therefore the names parameter is ignored in this case.
   *
   * When the tab is destroyed, the observable will complete.
   */
  getPageMetadata?
    (tabId: string, names: string[]): ObservableValue<PageMetadata>;

  /**
   * Returns an observable that emits when the browser wants the web client to
   * show a credential selection dialog.
   *
   * NOTE:
   * - The browser will only request one dialog at a time. We might have to
   * support concurrent PerformActions() in the future. The plan is to
   * sequence the requests.
   * - Currently the browser won't cancel the request. The task that issues the
   * request will yield and wait for the response, or fail the task when it
   * times out. The web client must also observe `getActorTaskState()` to clean
   * up the UI elements when the task is no longer active.
   */
  selectCredentialDialogRequestHandler?
    (): Observable<SelectCredentialDialogRequest>;

  /**
   * Returns an observable that emits when the browser wants the web client to
   * show a user confirmation dialog.
   *
   * NOTE:
   * - The browser will only request one dialog at a time. We might have to
   * support concurrent PerformActions() in the future. The plan is to
   * sequence the requests.
   * - Currently the browser won't cancel the request. The task that issues the
   * request will yield and wait for the response, or fail the task when it
   * times out. The web client must also observe `getActorTaskState()` to clean
   * up the UI elements when the task is no longer active.
   */
  selectUserConfirmationDialogRequestHandler?
    (): Observable<UserConfirmationDialogRequest>;

  /**
   * Returns an observable that emits when the browser wants the web client to
   * confirm a navigation to a novel origin with the model.
   *
   * NOTE:
   * - The browser will only make one request at a time. We might have to
   * support concurrent PerformActions() in the future. The plan is to
   * sequence the requests.
   * - Currently the browser won't cancel the request. The task that issues the
   * request will yield and wait for the response, or fail the task when it
   * times out. The web client must also observe `getActorTaskState()` to clean
   * up the UI elements when the task is no longer active.
   */
  selectNavigationConfirmationRequestHandler?
    (): Observable<NavigationConfirmationRequest>;

  /**
   * Returns an observable that emits when the browser wants the web client to
   * show an autofill suggestion selection dialog. The web client should
   * subscribe when actuating.
   *
   * NOTE:
   * - The browser will only request one dialog at a time. We might have to
   * support concurrent PerformActions() in the future. The plan is to
   * sequence the requests.
   * - Currently the browser won't cancel the request. The task that issues the
   * request will yield and wait for the response, or fail the task when it
   * times out. The web client must also observe `getActorTaskState()` to clean
   * up the UI elements when the task is no longer active.
   */
  selectAutofillSuggestionsDialogRequestHandler?
    (): Observable<SelectAutofillSuggestionsDialogRequest>;

  /**
   * Returns an observable that emits when the browser wants the web client to
   * show a Gmail OTP opt-in dialog.
   */
  selectGmailOtpOptInRequestHandler?(): Observable<GmailOtpOptInRequest>;

  /**
   * Switches to a use a different instance that shows the conversation
   * represented by the provided id. If `info` is not provided, a new instance
   * will be created with an empty conversation. When a new conversation is
   * created, the web client is expected to call `registerConversation` after
   * the first turn.
   *
   * If there are no other surfaces bound to the existing conversation, that
   * web client will be destroyed.
   *
   * The returned promise will resolve when the conversation switch is complete
   * on the browser side. The promise will be rejected if the switch fails.
   * The only possible error reason is `UNKNOWN`.
   */
  switchConversation?(info?: ConversationInfo): Promise<void>;

  /**
   * Registers a conversation in the web client.
   *
   * The returned promise will resolve when the conversation is successfully
   * registered with the browser. The promise will be rejected if registration
   * fails. Possible error reasons are:
   *  - `INSTANCE_ALREADY_HAS_CONVERSATION_ID`: The instance already has a
   *    conversation ID.
   *  - `UNKNOWN`: An unknown error occurred.
   */
  registerConversation?(info: ConversationInfo): Promise<void>;

  /**
   * Returns an observable that emits when additional context is available.
   */
  getAdditionalContext?(): Observable<AdditionalContext>;

  /**
   * Returns the host's capability to act on web pages. This reflects enterprise
   * policy for whether actuation is allowed.
   */
  getActOnWebCapability?(): ObservableValue<boolean>;

  /**
   * Called when the user has completed the onboarding flow.
   */
  setOnboardingCompleted?(): void;

  /**
   * Returns an observable that emits whether the user has completed the
   * onboarding flow. The observable will be updated when the value changes to
   * allow coordination between multiple Glic instances.
   */
  isOnboardingCompleted?(): ObservableValue<boolean>;

  /**
   * Returns an observable that emits when a user interacts with the actor task
   * list bubble and clicks on a task row (the observable emits the
   * corresponding task id).
   */
  actorTaskListRowClicked?(): Observable<number>;

  /**
   * Called when the microphone status changes in the web client.
   */
  onMicrophoneStatusChange?(status: MicrophoneStatus): void;

  /**
   * Informs Chrome of whether an error dialog is showing. Used for metrics,
   * and may cause Chrome to eventually reload the page if the GiC panel is
   * backgrounded.
   *
   * @param shownDialogType The type of error dialog that is showing. If
   *   `undefined`, no error dialog is showing.
   */
  setErrorDialogState?(shownDialogType?: ClientErrorDialogType): void;

  /**
   * Reports that the web client encountered a transient error. Transient errors
   * are errors may be presented to the user, but may not prevent further use of
   * GiC. Chrome may use this information to influence whether sign in cookies
   * are synced later.
   *
   * @param abslStatus A absl::StatusCode value. See
   *     https://abseil.io/docs/cpp/guides/status-codes.
   */
  reportClientTransientError?(abslStatus: number): void;

  /**
   * Notifies the host of a counter-abuse verdict received from the server.
   */
  processCounterAbuseVerdict?(tabId: string, verdict: CounterAbuseVerdict): void;
}

/** Information about a conversation. */

/** Fields of interest from the system settings page. */
export type OsPermissionType = 'media' | 'geolocation';

/** Holds optional parameters for `GlicBrowserHost#resizeWindow`. */
export declare interface ResizeWindowOptions {
  /**
   * If provided, `durationMs` will be used as the total duration of the resize
   * animation, in milliseconds. If not provided, the duration defaults to 0
   * (instant resizing). The promise will fail in case `durationMs` is not
   * finite.
   */
  durationMs?: number;
}

/**
 * Provides measurement-related functionality to the Glic web client.
 *
 * The typical sequence of events should be either:
 *  (onUserInputSubmitted -> (onResponseStarted ->
 *                            onResponseStopped)*
 *  )*
 * or
 *  onUserInputSubmitted -> onResponseStopped -> * (repeat)
 *
 * This is the core flow for metrics and the web client should do its best to
 * provide accurate and timely callbacks.
 *
 * The concepts of session and rating are less well defined. There are
 * intentionally no constraints on when or how often they are called.
 */
export declare interface GlicBrowserHostMetrics {
  /** Called when the opt-in CTA is shown. */
  onOptinImpression?(): void;

  /** Called when the user has submitted input via the web client. */
  onUserInputSubmitted?(mode: WebClientMode): void;

  /**
   * Called when the web client sends a browser actuation result over the
   * network. This is used to track metrics for model responses. For a single
   * actuation, this may be called multiple times if retries occur.
   * @param isRetry Whether this request is a retry of a previous attempt.
   */
  onPerformActionResultSubmitted?(isRetry?: boolean): void;

  /**
   * Called after user input is submitted, but before a response starts,
   * when the UI shows a message that explains the progress of the request.
   */
  onReaction?(reactionType: MetricUserInputReactionType): void;

  /** Called when starting to upload context to the server. */
  onContextUploadStarted?(): void;

  /** Called when finished uploading context to the server. */
  onContextUploadCompleted?(): void;

  /**
   * Called when the web client has sufficiently processed the input such that
   * it is able to start providing a response.
   */
  onResponseStarted?(): void;

  /**
   * Called when the response was completed, cancelled, or paused for the first
   * time.
   */
  onResponseStopped?(details?: OnResponseStoppedDetails): void;

  /** Called when a session terminates. */
  onSessionTerminated?(): void;

  /** Called when the user rates a response. */
  onResponseRated?(positive: boolean): void;

  /**
   * Called when the first caption is shown for the current request or response.
   * This can get fired multiple times in a single session.
   */
  onClosedCaptionsShown?(): void;

  /**
   * Called when a turn has been completed.
   */
  onTurnCompleted?(model: WebClientModel, duration: number): void;

  /**
   * Called when we want to record an use counter metric.
   */
  onRecordUseCounter?(action: WebUseCounter): void;

  // Removed fields and methods :
  onModelChanged?(): never;  // Last seen on Canary 146.0.7639.0
}

export enum ResponseStopCause {
  /** User cancelled response. */
  USER = 0,

  /** System cancelled response for another reason. */
  OTHER = 1,
}

/**
 * A rectangle with a position and size. All coordinate and size values are in
 * pixels.
 */
export declare interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

/**
 * A point with x and y coordinates. All coordinate values are in pixels.
 */
export declare interface Point {
  x: number;
  y: number;
}

/** An encoded journal. */
export declare interface Journal {
  /**
   * Encoded journal data. ArrayBuffer is transferable, so it should be copied
   * more efficiently over postMessage.
   */
  data: ArrayBuffer;
}

/**
 * Provides journal related functionality to the Glic web client.
 * This allows the web client to log entries into the journal and
 * to get a serialized capture (`snapshot`) of the journal.
 * To listen to new events to the journal `start` must
 * be called before any events can be serialized to the journal.
 * `start` does not need to be called before events are logged to
 * the journal as there may be other sinks of the journal that
 * wish to receive events.
 */
export declare interface GlicBrowserHostJournal {
  /**
   * Logs the start of an async event to the journal. A corresponding
   * endAsyncEvent must be called to terminate this event.
   */
  beginAsyncEvent(
    asynEventId: number, taskId: number, event: string,
    details: string): void;

  /**
   * Clears the contents of a started journal. No-op if a journal was not
   * started.
   */
  clear(): void;

  /**
   * Logs the end of an async event to the journal. A corresponding
   * `beginAsyncEvent` must have been previously called.
   */
  endAsyncEvent(asyncEventId: number, details: string): void;

  /**
   * Logs an instant event to the journal.
   */
  instantEvent(taskId: number, event: string, details: string): void;

  /**
   * Requests a snapshot of the current contents of the journal. Optionally
   * clear the journal after taking the snapshot.
   */
  snapshot(clear: boolean): Promise<Journal>;

  /**
   * Requests a journal to start logging. Calls to `snapshot`, `clear` or `stop`
   * can be made after this.
   */
  start(maxBytes: number, captureScreenshots: boolean): void;

  /**
   * Requests journal stop logging.
   */
  stop(): void;

  /**
   * Called when the user rates a response to submit a feedback with the current
   * journal snapshot.
   */
  recordFeedback?(positive: boolean, reason: string): void;
}

/** Data sent back to the host about the opening of the panel. */
export declare interface OpenPanelInfo {
  /** The mode in which the web client is opening at. */
  startingMode: WebClientMode;
  /**
   * @todo Make non-optional once the web client populates this value.
   *       https://crbug.com/390476866
   *
   * Panel sizing information that must be specified by the web client on every
   * open event. See documentation on `resizeWindow` for how the provided
   * arguments will be used.
   */
  resizeParams?: { width: number, height: number, options?: ResizeWindowOptions };

  /**
   * Whether the panel should start out resizable by the user. The panel is
   * resizable if this field is not provided.
   */
  canUserResize?: boolean;
}

/** The default value of TabContextOptions.pdfSizeLimit. */
export const DEFAULT_PDF_SIZE_LIMIT = 64 * 1024 * 1024;

/** The default value of TabContextOptions.innerTextBytesLimit. */
export const DEFAULT_INNER_TEXT_BYTES_LIMIT = 20000;

/**
 * Extension of TabContextResult to include an ActionResultCode while
 * maintaining backwards-compatibility.
 */
export declare interface ResumeActorTaskResult extends TabContextResult {
  // ActionResultCode that may have been supplied along with the
  // TabContextResult.
  // Note that this is an enum ActionResultCode from chrome/common/actor.mojom.
  // It is expected that the client has an equivalent enum definition. See
  // http://shortn/_gLyPxrRm6p
  actionResult?: number;
}

/** WARNING: See additional properties of TabData in the generated section */

/**
 * Data class holding information about the focused tab state. It works as a
 * discriminated union type: exactly one field is ever present.
 */
export declare interface FocusedTabData {
  /** Present only if a tab has focus. */
  hasFocus?: FocusedTabDataHasFocus;
  /** Present only if no tab has focus. */
  hasNoFocus?: FocusedTabDataHasNoFocus;
}

/** FocusedTabData variant for when a tab has focus. */
export declare interface FocusedTabDataHasFocus {
  /** Information about the focused tab. */
  tabData: TabData;
}

/** FocusedTabData variant for when no tabs have focus. */
export declare interface FocusedTabDataHasNoFocus {
  /**
   * Information about the active tab, which cannot be focused. Present only
   * if there is an active tab.
   */
  tabFocusCandidateData?: TabData;
  /** A human-readable message explaining why there is no focused tab. */
  noFocusReason: string;
}

/**
 * Annotates an image, providing security relevant information about the origins
 * from which image is composed.
 *
 * Note: This will be updated in the future when we have a solution worked out
 * for annotating the captured screenshots.
 */
export declare interface ImageOriginAnnotations { }

/** Maps the ErrorWithReason.reasonType to the type of reason. */
export declare interface ErrorReasonTypes {
  captureScreenshot: CaptureScreenshotErrorReason;
  captureRegion: CaptureRegionErrorReason;
  scrollTo: ScrollToErrorReason;
  webClientInitialize: WebClientInitializeErrorReason;
  actInFocusedTab: ActInFocusedTabErrorReason;
  createTask: CreateTaskErrorReason;
  performActions: PerformActionsErrorReason;
  switchConversation: SwitchConversationErrorReason;
  registerConversation: RegisterConversationErrorReason;
}

/** Reason why the web client could not initialize. */
export enum WebClientInitializeErrorReason {
  /**
   * Unknown reason. The user can manually retry loading, which reloads the
   * entire webview.
   */
  UNKNOWN = 0,
  /** This list will be expanded later. */
}

export type WebClientInitializeError = ErrorWithReason<'webClientInitialize'>;

/** Error implementation with a typed generic reason attached. */
export declare interface ErrorWithReason<
  T extends keyof ErrorReasonTypes> extends Error {
  /** A tag that identifies the reason type. */
  reasonType: T;
  /** The reason for the error. */
  reason: ErrorReasonTypes[T];
}

/** Reason for failure while acting in the focused tab. */
export enum ActInFocusedTabErrorReason {
  UNKNOWN = 0,
  /** Context could not be gathered after acting. */
  GET_CONTEXT_FAILED = 1,
  /** The action proto is invalid. */
  INVALID_ACTION_PROTO = 2,
  /** Action target is not found. */
  TARGET_NOT_FOUND = 3,
  /** Failed to start a new task. */
  FAILED_TO_START_TASK = 4,
}

/** @deprecated Use `performAction` instead. */
export declare interface ActInFocusedTabResult {
  // The tab context result after acting and gathering new context.
  tabContextResult?: TabContextResult;
  // The outcome of the action.
  // Note that this is an enum ActionResultCode from chrome/common/actor.mojom.
  // It is expected that the client has an equivalent enum definition. See
  // http://shortn/_gLyPxrRm6p
  actionResult?: number;
}

/** @deprecated Use `performAction` instead. */
export declare interface ActInFocusedTabParams {
  // Corresponds to
  // components/optimization_guide/proto/features/actions_data.proto:
  // BrowserAction
  actionProto: ArrayBuffer;
  // Tab context options to gather context after acting.
  tabContextOptions: TabContextOptions;
}

/** Error type used for screenshot capture errors. */
export type CaptureScreenshotError = ErrorWithReason<'captureScreenshot'>;

export type CaptureRegionError = ErrorWithReason<'captureRegion'>;

/** Error type used for actuation errors. */
export type ActInFocusedTabError = ErrorWithReason<'actInFocusedTab'>;

/** Error type used for create task errors. */
export type CreateTaskError = ErrorWithReason<'createTask'>;

/** Error type used for perform actions errors. */
export type PerformActionsError = ErrorWithReason<'performActions'>;

/** Error type used for scrollTo(). */
export type ScrollToError = ErrorWithReason<'scrollTo'>;

/**
 * A generic interface for observing a stream of values.
 *
 * Subscriptions should be kept only while necessary, as they incur some cost.
 * When not needed anymore, call Subscriber.unsubscribe() on the instance
 * returned by subscribe.
 */
export declare interface Observable<T> {
  /** Receive updates for value changes. */
  subscribe(change: (newValue: T) => void): Subscriber;

  /**
   * Subscribe with an Observer.
   * This API was added in later, and is not supported by all versions of
   * Chrome.
   */
  subscribeObserver?(observer: Observer<T>): Subscriber;
}

/**
 * A generic interface for observing a stream of values.
 *
 * Unlike `Observable`, `subscribeObserver` is required and in the future the
 * two related Observable interfaces will be merged into one.
 *
 * Subscriptions should be kept only while necessary, as they incur some cost.
 * When not needed anymore, call Subscriber.unsubscribe() on the instance
 * returned by subscribe.
 */
export declare interface Observable2<T> {
  /** Receive updates for value changes. */
  subscribe(change: (newValue: T) => void): Subscriber;

  /**
   * Subscribe with an Observer.
   * This API was added in later, and is not supported by all versions of
   * Chrome.
   */
  subscribeObserver(observer: Observer<T>): Subscriber;
}

/**
 * An observable value that may change over time. A subscriber is guaranteed to
 * be called once with the value, and again anytime the value changes. Note that
 * the subscriber may or may not be invoked immediately upon calling
 * subscribe().
 *
 * See also comments about Observable.
 */
export declare interface ObservableValue<T> extends Observable<T> {
  /**
   * Provides synchronous access to the current value. Returns undefined if the
   * initial value has not yet been populated.
   */
  getCurrentValue(): T | undefined;
}

/** Allows control of a subscription to an Observable. */
export declare interface Subscriber {
  unsubscribe(): void;
}

/** Observes an Observable. */
export declare interface Observer<T> {
  /** Called when the Observable emits a value. */
  next?(value: T): void;
  /** Called if the Observable emits an error. */
  error?(err: unknown): void;
  /** Called when the Observable completes. */
  complete?(): void;
}


/** Chrome version data broken down into its numeric components. */
export declare interface ChromeVersion {
  major: number;
  minor: number;
  build: number;
  patch: number;
}

//
// Types used in the boot process.
//

/** Allows access to the injected boot function. */
export declare interface WithGlicApi {
  internalAutoGlicBoot?(chromeSource: WindowProxy): GlicHostRegistry;
}

/** Message used to signal a boot injection request to the host. */
export declare interface GlicApiBootMessage {
  type: 'glic-bootstrap';
  glicApiSource: string;
}

export declare interface UserConfirmationDialogRequest {
  // If present, the actor is requesting the user confirm that it can
  // navigate or act on the provided origin.
  navigationOrigin?: string;
  // If present, true when the navigationOrigin in a request is on the
  // Optimization Guide blocklist.
  forBlocklistedOrigin?: boolean;

  /**
   * @deprecated Unique integer ID for identifying downloads
   * for confirmation. We decided not to show user confirmation
   * dialog in that case.
   */
  downloadId?: number;

  // The WebClient must call this function to respond back to the browser when
  // the dialog is closed.
  onDialogClosed(result: { response: UserConfirmationDialogResponse }): void;
}

export declare interface UserConfirmationDialogResponse {
  // The verdict of the user confirmation dialog.
  permissionGranted: boolean;
}

export declare interface NavigationConfirmationRequest {
  // ID of the actor's task.
  taskId: number;
  // Origin to request the actor navigate to.
  navigationOrigin: string;

  // The WebClient must call this function to respond back to the browser when
  // the confirmation request has a decision.
  onConfirmationDecision(result: { response: NavigationConfirmationResponse }):
    void;
}

export declare interface NavigationConfirmationResponse {
  // The verdict of the model if the actor can navigate to this origin.
  permissionGranted?: boolean;
}

export declare interface GmailOtpOptInRequest {
  // ID of the actor's task.
  taskId: number;

  // The WebClient must call this function to respond back to the browser when
  // the dialog is closed.
  onDialogClosed(response: GmailOtpOptInResponse): void;
}

export declare interface GmailOtpOptInResponse {
  // True if the user clicked the opt-in button, false if they
  // cancelled/closed it.
  permissionGranted: boolean;
}

/**
 * The response from the web client containing the chosen suggestions for each
 * form.
 */
export declare interface SelectAutofillSuggestionsDialogResponse {
  /**
   * The IDs of the selected suggestions. The order of IDs in this list
   * corresponds to the order of `requests` in the
   * `SelectAutofillSuggestionsDialogRequest`.
   */
  selectedSuggestions: FormFillingResponse[];
}

import './glic_api_generated.js';

// Manual additions to generated types are defined here.
declare module './glic_api_generated.js' {
  export interface PanelOpeningData {
    /** @deprecated Use `conversationInfo` instead. */
    conversationId?: string;
  }

  /**
   * A request for the web client to show suggestion selectors for a number of
   * forms.
   */
  export interface SelectAutofillSuggestionsDialogRequest {
    /**
     * The WebClient must call this function to respond back to the browser when
     * the dialog is closed.
     */
    onDialogClosed(result: {response: SelectAutofillSuggestionsDialogResponse}):
        void;

    /** Called when a form's suggestions are presented in the UI. */
    onFormPresented?(params: {formFillingRequestIndex: number}): void;

    /**
     * Called when a preview is requested (e.g. by hovering over a suggestion).
     * `response` is undefined when no preview is to be shown (e.g. moving the
     * mouse away from the suggestion).
     */
    onFormPreviewChanged?(params: {
      formFillingRequestIndex: number,
      response?: FormFillingResponse,
    }): void;

    /** Called when the user has confirmed a selection. */
    onFormConfirmed?(params: {
      formFillingRequestIndex: number,
      response: FormFillingResponse,
    }): void;
  }

  /** Credential selection dialog. */
  export interface SelectCredentialDialogRequest {
    // The WebClient must call this function to respond back to the browser when
    // the dialog is closed.
    onDialogClosed(result: {response: SelectCredentialDialogResponse}): void;
  }

  export interface TabData {
    /**
     * Returns the favicon for the tab, encoded as a PNG image. An image is
     * returned only if the page is loaded enough for it to be available and the
     * page specifies a favicon.
     *
     * @deprecated Use `getTabFaviconById` instead. This does not work on
     *     Android.
     * Favicons may be omitted if the client capability
     * `IGNORES_TAB_DATA_FAVICONS` is present on this instance.
     */
    favicon?(): Promise<Blob|undefined>;
  }

  /** Information from a signed-in Chrome user profile. */
  export interface UserProfileInfo {
    /**
     * Returns the avatar icon for the profile, if available. Encoded as a PNG
     * image.
     */
    avatarIcon(): Promise<Blob|undefined>;
  }

  /** Represents a single skill preview. */
  export interface SkillPreview {
    /** Whether the skill is contextually relevant to the current tab. */
    isContextual?: boolean;
  }

  /** A credential used for the auto-login. */
  export interface Credential {
    // The optional icon for the credential, encoded as a PNG image.
    // For federated credentials, this is the brand icon of the identity
    // provider.
    getIcon?(): Promise<Blob>;
    // For federated credentials, an optional picture for the account, provided
    // by the identity provider, encoded as a PNG image. Not provided for
    // password based credentials.
    getAccountPicture?(): Promise<Blob>;
  }

  /** A single autofill suggestion for a form. */
  export interface AutofillSuggestion {
    /** The optional icon for the suggestion, encoded as a PNG image. */
    getIcon?(): Promise<Blob>;
  }
}

//
// Types used in presubmit check.
//

// Types not intended to be used externally, and therefore may not be
// backwards compatible. All remaining types can only be updated in
// backwards compatible ways.
export interface PrivateTypes {
  privateTypes: PrivateTypes;
  closedEnums: ClosedEnums;
}

// Enums that should not be changed. All other enums may be extended
// in future versions.
export interface ClosedEnums {
  panelStateKind: typeof PanelStateKind;
  webClientMode: typeof WebClientMode;

  // NOTICE: Enums below this line were added here by default, and
  // may in fact be safe to extend. Please verify safety before
  // removing them.
  webClientModel: typeof WebClientModel;
  skillSource: typeof SkillSource;
  switchConversationErrorReason: typeof SwitchConversationErrorReason;
  pinTrigger: typeof PinTrigger;
  registerConversationErrorReason: typeof RegisterConversationErrorReason;
  metricUserInputReactionType: typeof MetricUserInputReactionType;
  unpinTrigger: typeof UnpinTrigger;
  responseStopCause: typeof ResponseStopCause;
}
