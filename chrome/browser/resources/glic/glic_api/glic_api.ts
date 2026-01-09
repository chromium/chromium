// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API between the Chrome browser and the Glic web client.
//
// Overall notes:
// - There may be multiple instances of the web client running at a time, all
//   sharing the same web storage space. Whenever one is started or restarted,
//   the initialization steps will be repeated.
// - As in TypeScript all `number`s are 64 bit floating points, we decided to
//   make all identifier values be of the `string` type (e.g. for a window or a
//   tab).
// - The defined functions and interfaces can be "evolved" to provide more
//   functionality and data, as needed, but must be kept backwards compatible.
// - Functions are documented with their known behavior. Exceptions and promise
//   failures should be documented only if they are expected.
// - The browser provided tab and window IDs are based on the browser's
//   SessionID values, which are not stable between Chrome restarts, and should
//   not be saved to persisted storage for later reuse. See:
//   https://crsrc.org/c/components/sessions/core/session_id.h
// - URLs can be arbitrarily long but any URL sent or received through this API
//   will be silently made empty if exceeding the 2 MiB length limit imposed by
//   Mojo's URL implementation. See:
//   https://crsrc.org/c/url/mojom/url.mojom

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

/** Additional context object. */
export declare interface AdditionalContext {
  /** User facing name of the context.  Eg. the filename, or full url */
  name?: string;

  /**
   * Tab id, if associated with a tab.
   * Callers may use this to associate context but should not assume this
   * relationship persists as tab contents change.
   */
  tabId?: string;

  /** Origin of the frame where the data came from (if it came from a frame) */
  origin?: string;

  /** url of the frame where the data came from (if it came from a frame) */
  frameUrl?: string;

  /** The parts of the context. */
  parts: AdditionalContextPart[];
}

/** Part of an additional context object. Only one field will be present. */
export declare interface AdditionalContextPart {
  /**
   * The context data. The MIME type is available from the `type` property.
   * Callers can use `arrayBuffer()` to get the data as a buffer, or `stream()`
   * to read it as a stream if the data is large.
   */
  data?: Blob;
  /**
   * The following four fields can be contained by `tabContext` and are
   * deprecated
   */
  screenshot?: Screenshot;
  webPageData?: WebPageData;
  annotatedPageData?: AnnotatedPageData;
  pdf?: PdfDocumentData;
  tabContext?: TabContextResult;
}

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
      (panelOpeningData: PanelOpeningData&PanelState): Promise<OpenPanelInfo>;

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

  // !!! ATTENTION !!!
  // Avoid adding new methods to this interface! Instead, to push information to
  // the web client it's much more preferable to add new functions to
  // GlicBrowserHost that return an Observable or ObservableValue instances.
}

/**
 * Provides functionality implemented by the browser to the Glic web client.
 * Most functions are optional.
 */
export declare interface GlicBrowserHost {
  /** Returns the precise Chrome's version. */
  getChromeVersion(): Promise<ChromeVersion>;

  /** Return the platform glic is running on. */
  getPlatform?(): Platform;

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
   * Set the areas of the glic window from which it should be draggable. If
   * `areas` is empty, a default draggable area will be created.
   *
   * Returns a promise that resolves when the browser has updated the draggable
   * area.
   */
  setWindowDraggableAreas(areas: DraggableArea[]): Promise<void>;

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
   */
  interruptActorTask?(taskId: number): void;

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
   * Note that updates are only sent for a subset of changes to the tab.
   *
   * WARNING: The current implementation within Chrome makes this unsuitable
   * for general use. Only tabs involved with actor tasks are supported. The
   * observable remains open even if there's no tab.
   * @todo Generalize this to work with non-actor tabs.
   * @todo Complete the observable when tabs are removed.
   */
  getTabById?(tabId: string): ObservableValue<TabData>;

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
  captureRegion?(): ObservableValue<CaptureRegionResult>;

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

  /** Returns the state of the tab context permission. */
  getTabContextPermissionState?(): ObservableValue<boolean>;

  /** Returns the state of the OS granted location permission. */
  getOsLocationPermissionState?(): ObservableValue<boolean>;

  /** Returns the state of the OS hotkey. */
  getOsHotkeyState?(): ObservableValue<{hotkey: string}>;

  /** Returns the state of the glic closed captioning setting. */
  getClosedCaptioningSetting?(): ObservableValue<boolean>;

  /** Returns the state of the web actuation setting. */
  getActuationOnWebSetting?(): ObservableValue<boolean>;

  /**
   * Returns the state of the default tab context permission for new sessions.
   * The returned observable will be updated when the setting changes.
   */
  getDefaultTabContextPermissionState?(): ObservableValue<boolean>;

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
   * Set the state of the tab context permission in settings. Returns a promise
   * that resolves when the browser has stored the new pref value.
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
   */
  pinTabs?(tabIds: string[]): Promise<boolean>;

  /**
   * Attempts to unpin the given tabs. Can fail if the any of the tabs cannot be
   * found, or if the tab isn't pinned. Return value is true if all tabs were
   * unpinned. A false value does not mean that no tabs were unpinned. The
   * updated set of pinned tabs will asynchronously be available via
   * getPinnedTabs.
   */
  unpinTabs?(tabIds: string[]): Promise<boolean>;

  /**
   * Unpins all currently pinned tabs.
   */
  unpinAllTabs?(): void;

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
   * Returns the list of capabilities of the glic host.
   */
  getHostCapabilities?(): Set<HostCapability>;

  /**
   * Emits when the browser wants the web client to change its view to match
   * a requested change (e.g., because the user clicked a UI element to toggle
   * to a different view).
   *
   * The web client should update its view to match the requested change.
   */
  getViewChangeRequests?(): Observable<ViewChangeRequest>;

  /**
   * Notifies the browser that the web client has changed the view shown to the
   * user. This is used to trigger updates to browser UI which shows the current
   * state of the web client, such as toggle controls.
   */
  onViewChanged?(notification: ViewChangedNotification): void;

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

  /** Returns the host's capability to act on web pages. */
  getActOnWebCapability?(): ObservableValue<boolean>;

  /**
   * @todo Not yet implemented: https://crbug.com/458761731.
   *
   * Load and extract content from given urls.
   */
  loadAndExtractContent?(urls: string[], options: TabContextOptions[]):
      Promise<TabContextResult[]>;
}

/** Information about a conversation. */
export declare interface ConversationInfo {
  /** The unique ID of the conversation. This will be stored. */
  conversationId: string;
  /**
   *  The title of the conversation. This will be stored. It is expected that
   *  titles don't change.
   */
  conversationTitle: string;
}

/** Fields of interest from the system settings page. */
export type OsPermissionType = 'media'|'geolocation';

/** Optional parameters for the openGlicSettingsPage function. */
export declare interface OpenSettingsOptions {
  /**
   * Optionally select a field to be highlighted while opening the Glic settings
   * page.
   */
  highlightField?: SettingsPageField;
}

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

/** Holds optional parameters for `GlicBrowserHost#createTab`. */
export declare interface CreateTabOptions {
  /** Determines if the new tab should be created in the background or not. */
  openInBackground?: boolean;
  /** The windowId of the window where the new tab should be created at. */
  windowId?: string;
}

/** Holds optional parameters for `GlicBrowserHost#createActorTab`. */
export declare interface CreateActorTabOptions {
  /** The tabId of the tab from which the conversation turn was initiated. */
  initiatorTabId?: string;
  /**
   * The windowId of the window which the conversation turn was initiated.
   * This may differ from the initiatorTabId's current window if the tab is
   * moved to a different window or closed.
   */
  initiatorWindowId?: string;
  /**
   * Determines if the new tab should be created in the background or not. If
   * not provided, defaults to `false`.
   */
  openInBackground?: boolean;
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
  /** Called when the user has submitted input via the web client. */
  onUserInputSubmitted?(mode: WebClientMode): void;

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
   * Called when the model is changed. Metrics may be recorded with a separate
   * scope.
   */
  onModelChanged?(model: WebClientModel): void;

  /**
   * Called when we want to record an use counter metric.
   */
  onRecordUseCounter?(action: WebUseCounter): void;
}

export enum ResponseStopCause {
  /** User cancelled response. */
  USER = 0,

  /** System cancelled response for another reason. */
  OTHER = 1,
}

/** Details for metrics recording purposes. */
export declare interface OnResponseStoppedDetails {
  cause?: ResponseStopCause;
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
 * A region captured by the user from a document in a tab.
 *
 * This is a union of different possible region shapes. Currently only
 * rectangular regions are supported, but this may be expanded with other region
 * types like polygons in the future.
 */
export declare interface CapturedRegion {
  /**
   * A rectangular region captured from a document in a tab.
   *
   * The coordinate system is relative to the top-left corner of the document.
   * The units are in pixels and match screenshot pixel dimensions.
   *
   * - **Position (`x`, `y`):** Coordinates of the top-left corner of the
   *   rectangle, relative to the document's origin (0,0). Can be negative if
   *   content is scrolled out of view.
   * - **Size (`width`, `height`):** Dimensions of the rectangle, expected to be
   *   non-negative.
   *
   * The rectangle can represent an area outside the currently visible viewport
   * if the page is scrolled. It is not guaranteed to be contained within the
   * document's bounds.
   */
  rect?: Rect;
}

/** The result of a successful region capture. */
export declare interface CaptureRegionResult {
  /** The ID of the tab from which the region was captured. */
  tabId?: string;
  /**
   * The captured region. This can be expanded with other region types like
   * polygons in the future.
   */
  region?: CapturedRegion;
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
  resizeParams?: {width: number, height: number, options?: ResizeWindowOptions};

  /**
   * Whether the panel should start out resizable by the user. The panel is
   * resizable if this field is not provided.
   */
  canUserResize?: boolean;
}

/**
 * Information of how the panel is being presented/configured.
 */
export declare interface PanelState {
  /**
   * The panel's presentation kind/state.
   */
  kind: PanelStateKind;
  /**
   * Present only when attached to a window, indicating which window it is
   * attached to.
   */
  windowId?: string;
}

/**
 * Data structure sent from the browser to the web client with panel opening
 * information.
 */
export declare interface PanelOpeningData {
  /**
   * The state of the panel as it's being opened.
   */
  panelState?: PanelState;
  /**
   * Indicates the entry point used to trigger the opening of the panel.
   * In the event the web client's page is reloaded, the new web client will
   * receive a notifyPanelWillOpen call with the same invocation source as
   * before, even though the user did not, for example, click a button again.
   */
  invocationSource?: InvocationSource;
  /**
   * The ID of the conversation to open. If unset, the web client will open a
   * new conversation. This field is used only when the `MULTI_INSTANCE`
   * capability is present.
   */
  conversationId?: string;
  /**
   * If set, the textbox for user input will be populated with the given string
   * before the panel opens.
   */
  promptSuggestion?: string;
  /**
   * Up to 3 most recently active conversations, ordered by most recently active
   * first.
   */
  recentlyActiveConversations?: ConversationInfo[];
}

/** The default value of TabContextOptions.pdfSizeLimit. */
export const DEFAULT_PDF_SIZE_LIMIT = 64 * 1024 * 1024;

/** The default value of TabContextOptions.innerTextBytesLimit. */
export const DEFAULT_INNER_TEXT_BYTES_LIMIT = 20000;

/** Options for getting context from a tab. */
export declare interface TabContextOptions {
  /**
   * If true, an innerText representation of the page will be included in the
   * response.
   */
  innerText?: boolean;
  /**
   * Maximum size in UTF-8 bytes that the returned innerText data may contain.
   * If exceeded, the innerText will be truncated to the nearest character that
   * will leave the string less than or equal to the specified byte size.
   * Defaults to DEFAULT_INNER_TEXT_BYTES_LIMIT. If it is zero or negative,
   * the innerText will be empty.
   */
  innerTextBytesLimit?: number;
  /**
   * If true, a screenshot of the user visible viewport will be included in the
   * response.
   */
  viewportScreenshot?: boolean;
  /** If true, returns the serialized annotatedPageContent proto. */
  annotatedPageContent?: boolean;
  /**
   * Maximum number of meta tags (per Document/Frame) to include in the
   * response. Defaults to 0 if not provided.
   */
  maxMetaTags?: number;
  /**
   * If true, and the focused tab contains a PDF as the top level document,
   * returns PdfDocumentData.
   */
  pdfData?: boolean;
  /**
   * Maximum size in bytes for returned PDF data. If this size is exceeded,
   * PdfDocumentData is still returned, but it will not contain PDF bytes.
   * Defaults to DEFAULT_PDF_SIZE_LIMIT. If it is zero or negative, PDF bytes
   * will never be returned.
   */
  pdfSizeLimit?: number;
  /**
   * The mode of the annotated page content if included in the response. This
   * maps directly to the AnnotatedPageContentMode enum in the proto.
   */
  annotatedPageContentMode?: number;
}

/**
 * Data class holding information and contents extracted from a tab.
 */
export declare interface TabContextResult {
  /** Metadata about the tab that holds the page. Always provided. */
  tabData: TabData;
  /**
   * Information about a web page rendered in the tab at its current state.
   * Provided only if requested.
   */
  webPageData?: WebPageData;
  /**
   * A screenshot of the user-visible portion of the page. Provided only if
   * requested.
   */
  viewportScreenshot?: Screenshot;
  /**
   * PDF document data. Provided if requested, and the top level document in the
   * focused tab is a PDF.
   */
  pdfDocumentData?: PdfDocumentData;
  /** Page content data. Provided if requested. */
  annotatedPageData?: AnnotatedPageData;
}

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

/**
 * Used for customizing the list of pin candidates.
 */
export declare interface GetPinCandidatesOptions {
  /** The maximum number of candidates to consider. Can return fewer. */
  maxCandidates: number;
  /** A query string. */
  query?: string;
}

/** Information about a web page being rendered in a tab. */
export declare interface WebPageData {
  mainDocument: DocumentData;
}

/** Information about a PDF document. */
export declare interface PdfDocumentData {
  /** Origin of the document. */
  origin: string;
  /** Raw PDF data, if it could be obtained. */
  pdfData?: ReadableStream<Uint8Array>;
  /**
   * Whether the PDF size limit was exceeded. If true, `pdfData` will be empty.
   */
  pdfSizeLimitExceeded: boolean;
}

/** Text information about a web document. */
export declare interface DocumentData {
  /** Origin of the document. */
  origin: string;
  /**
   * The innerText of the document at its current state. Currently includes
   * embedded same-origin iframes.
   */
  innerText?: string;

  /** Whether `innerText` was truncated due to `innerTextBytesLimit`. */
  innerTextTruncated?: boolean;
}

/** Annotated data from a web document. */
export declare interface AnnotatedPageData {
  /** Serialized annotatedPageContent proto. */
  annotatedPageContent?: ReadableStream<Uint8Array>;
  /**
   * Metadata about the annotated page content. Present when
   * annotatedPageContent has been requested.
   */
  metadata?: PageMetadata;
}

/** Meta tag name and content taken from the <head> element of a frame. */
export declare interface MetaTag {
  name: string;
  content: string;
}

/**
 * Metadata about a frame.  Number of MetaTags is limited by the
 * maxMetaTags option.
 */
export declare interface FrameMetadata {
  url: string;
  metaTags: MetaTag[];
}

/** Metadata about the page.  Includes URL and meta tags for each frame. */
export declare interface PageMetadata {
  frameMetadata: FrameMetadata[];
}

/**
 * Various bits of data about a browser tab. Optional fields may not be
 * available while the page is being loaded or if not provided by the page
 * itself.
 */
export declare interface TabData {
  /**
   * Unique ID of the tab that owns the page. These values are unique across
   * all tabs from all windows, and will not change even if the user moves the
   * tab to a different window.
   */
  tabId: string;
  /**
   * Unique ID of the browser window holding the tab. This value may change if
   * the tab is moved to a different window.
   */
  windowId: string;
  /**
   * URL of the page. For a given tab, this value will change if the tab is
   * navigated to a different URL.
   */
  url: string;
  /**
   * The title of the loaded page. Returned only if the page is loaded enough
   * for it to be available. It may be empty if the page did not specify a
   * title.
   */
  title?: string;
  /**
   * Returns the favicon for the tab, encoded as a PNG image. An image is
   * returned only if the page is loaded enough for it to be available and the
   * page specifies a favicon.
   */
  favicon?(): Promise<Blob|undefined>;
  /**
   * The favicon URL. Only available if the page is loaded enough and it
   * specifies a favicon.
   *
   * @todo Investigate render performance of data urls. crbug.com/429237829
   */
  faviconUrl?: string;
  /**
   * MIME type of the main document. Returned only if the page is loaded enough
   * for it to be available.
   */
  documentMimeType?: string;
  /**
   * Whether the tab is audible or visible. Specifically this is the visibility
   * of the WebContents as returned by: `WebContents::GetVisibility`. If the
   * visibility is either VISIBLE or OCCLUDED, we consider the web contents to
   * be visible. @todo: This field is being added as a temporary solution.
   * b/433995475
   */
  isObservable?: boolean;

  /**
   * Whether the tab has active audio or video playing, used for showing tab UI.
   * This is a best effort signal, and may not be accurate/stale due to not
   * observing media events directly. @todo: This field is being added as a
   * temporary solution. b/433995475
   */
  isMediaActive?: boolean;

  /**
   * Whether the tab content is being captured by another functionality (e.g.,
   * screen share in video chat). This is a best effort signal, and may not be
   * accurate/stale due to not observing tab content capture events
   * directly. @todo: This field is being added as a temporary solution.
   * b/433995475
   */
  isTabContentCaptured?: boolean;

  /**
   * Whether the tab is the active tab in its browser window. Note that this
   * does not consider the state of the window.
   */
  isActiveInWindow?: boolean;

  /**
   * Whether the tab's browser window is active. Note that this does not
   * consider whether the tab is active in the window.
   */
  isWindowActive?: boolean;
}

/** A candidate for pinning. */
export declare interface PinCandidate {
  /** The tab that is a candidate for pinning. */
  tabData: TabData;
}

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
export declare interface ImageOriginAnnotations {}

/**
 * An encoded screenshot image and associated metadata.
 */
export declare interface Screenshot {
  /** Width and height of the image in pixels. */
  widthPixels: number;
  heightPixels: number;
  /**
   * Encoded image data. ArrayBuffer is transferable, so it should be copied
   * more efficiently over postMessage.
   */
  data: ArrayBuffer;
  /** The image encoding format represented as a MIME type. */
  mimeType: string;
  /** Image annotations for this screenshot. */
  originAnnotations: ImageOriginAnnotations;
}

/**
 * Contains information about the task.
 */
export declare interface TaskOptions {
  /**
   * A user-facing string that describes the task.
   */
  title?: string;
}

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

export declare interface ActInFocusedTabResult {
  // The tab context result after acting and gathering new context.
  tabContextResult?: TabContextResult;
  // The outcome of the action.
  // Note that this is an enum ActionResultCode from chrome/common/actor.mojom.
  // It is expected that the client has an equivalent enum definition. See
  // http://shortn/_gLyPxrRm6p
  actionResult?: number;
}

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

/** Params for scrollTo(). */
export declare interface ScrollToParams {
  /**
   * Whether we should highlight the content selected. True by default if not
   * specified. If false, the content is scrolled to but not highlighted.
   */
  highlight?: boolean;

  /** Used to specify content to scroll to and highlight. */
  selector: ScrollToSelector;

  /**
   * Identifies the document we want to perform the scrollTo operation on. When
   * specified, we verify that the currently focused tab's document matches the
   * ID, and throw an error if doesn't. This is a required parameter for all
   * document types except PDF (see `url` below), and a NOT_SUPPORTED error will
   * be thrown if it is not specified.
   */
  documentId?: string;

  /**
   * Identifies the url of a document we want to perform the scrollTo
   * operation on. This is only required when scrolling PDF documents (and is
   * ignored otherwise; other document types require `documentId` to be
   * specified instead), and is used to verify that the currently focused tab
   * still points to a PDF with that URL. If not specified, and the currently
   * focused tab has a PDF loaded, a NOT_SUPPORTED error will be thrown.
   */
  url?: string;
}

/**
 * Used to select content to scroll to. Note that only one concrete selector
 * type can be present.
 * Additional selector types will be added to this API in the future.
 */
export declare interface ScrollToSelector {
  /** Exact text selector, see ScrollToTextSelector for more details. */
  exactText?: ScrollToTextSelector;

  /**
   * Text fragment selector, see ScrollToTextFragmentSelector for more details.
   */
  textFragment?: ScrollToTextFragmentSelector;

  /** Node selector, see ScrollToNodeSelector for more details. */
  node?: ScrollToNodeSelector;
}

/**
 * scrollTo() selector to select exact text in HTML and PDF documents within
 * a given search range starting from the start node (specified with
 * searchRangeStartNodeId) to the end of the document. If not specified, the
 * search range will be the entire document.
 * The documentId in ScrollToParams must be specified if a
 * searchRangeStartNodeId is specified.
 */
export declare interface ScrollToTextSelector {
  text: string;

  /**
   * See common_ancestor_dom_node_id in proto ContentAttributes
   * in components/optimization_guide/proto/features/common_quality_data.proto.
   */
  searchRangeStartNodeId?: number;
}

/**
 * scrollTo() selector to select a range of text in HTML and PDF documents
 * within a given search range starting from the start node (specified with
 * searchRangeStartNodeId) to the end of the document. If not specified, the
 * search range will be the entire document.
 * The documentId in ScrollToParams must be specified if a
 * searchRangeStartNodeId is specified.
 * Text selected will match textStart <anything in the middle> textEnd.
 */
export declare interface ScrollToTextFragmentSelector {
  textStart: string;
  textEnd: string;

  /**
   * See common_ancestor_dom_node_id in proto ContentAttributes
   * in components/optimization_guide/proto/features/common_quality_data.proto.
   */
  searchRangeStartNodeId?: number;
}

/**
 * scrollTo() selector to select all text inside a specific node (corresponding
 * to the provided nodeId). documentId must also be specified in ScrollToParams
 * when this selector is used.
 */
export declare interface ScrollToNodeSelector {
  /**
   * Value should be obtained from common_ancestor_dom_node_id in
   * ContentAttributes (see
   * components/optimization_guide/proto/features/common_quality_data.proto)
   */
  nodeId: number;
}

/** Error type used for scrollTo(). */
export type ScrollToError = ErrorWithReason<'scrollTo'>;

/**
 * A rectangular area based in the glic window's coordinate system. All
 * coordinate and size values are in DIPs. The coordinate system is based in the
 * panel's view with the origin located in the top-left of the panel.
 */
export declare interface DraggableArea {
  x: number;
  y: number;
  width: number;
  height: number;
}

/**
 * Top-level views of the glic web client.
 */
export enum ClientView {
  ACTUATION = 'actuation',
  CONVERSATION = 'conversation',
}

/**
 * A request to change the glic web client to a view suitable for tracking the
 * progress of actuation, if possible.
 */
export declare interface ViewChangeRequestActuation {
  readonly desiredView: ClientView.ACTUATION;
}

/**
 * A request to change the glic web client to a view which shows a
 * conversational interface of some type (whether textual, aural or other).
 */
export declare interface ViewChangeRequestConversation {
  readonly desiredView: ClientView.CONVERSATION;
}

/**
 * A request to change the glic web client to a view of some type. These all
 * specify what the desired view is, but some may carry additional information
 * about the request.
 */
export declare type ViewChangeRequest =
    ViewChangeRequestActuation | ViewChangeRequestConversation;

/**
 * A notification that the view has changed to the specified view.
 */
export declare interface ViewChangedNotification {
  /** The view that was changed to. */
  currentView: ClientView;
}

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
 * An observable value that may change over time. A subscriber is guaranteed to
 * be called once with the value, and again anytime the value changes. Note that
 * the subscriber may or may not be invoked immediately upon calling
 * subscribe().
 *
 * See also comments about Observable.
 */
export interface ObservableValue<T> extends Observable<T> {
  /**
   * Provides synchronous access to the current value. Returns undefined if the
   * initial value has not yet been populated.
   */
  getCurrentValue(): T|undefined;
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
  error?(err: any): void;
  /** Called when the Observable completes. */
  complete?(): void;
}

/** Information from a signed-in Chrome user profile. */
export declare interface UserProfileInfo {
  /**
   * Returns the avatar icon for the profile, if available. Encoded as a PNG
   * image.
   */
  avatarIcon(): Promise<Blob|undefined>;
  /** The full name displayed for this profile. */
  displayName: string;
  /** The given name for this profile. */
  givenName?: string;
  /** The local profile name, which can be customized by the user. */
  localProfileName?: string;
  /** The profile email. */
  email: string;
  /** Whether the profile's signed-in account is a managed account. */
  isManaged?: boolean;
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

/** Zero-state suggestions for the current tab context. */
export declare interface ZeroStateSuggestionsV2 {
  /**
   * A collection of suggestions associated with current tab context. This may
   * be empty.
   */
  suggestions: SuggestionContent[];
  /**
   * Whether there is a current outstanding request to generate suggestions for
   * the current tab context.
   */
  isPending?: boolean;
}

/**
 * Options for ensuring chrome will create Zero State Suggestions for a
 * specific webui context.
 */
export declare interface ZeroStateSuggestionsOptions {
  /** If the suggestions will be used in a first run context. */
  isFirstRun?: boolean;
  /** The list of tools that are currently supported. */
  supportedTools?: string[];
}

/** Zero-state suggestions for the current tab. */
export declare interface ZeroStateSuggestions {
  /**
   * A collection of suggestions associated with the linked tab. This may be
   * empty.
   */
  suggestions: SuggestionContent[];
  /** A unique ID to track the the associated tab. */
  tabId: string;
  /** The url of the associated tab. */
  url: string;
}

/** Zero-state suggestion for the current tab.*/
export declare interface SuggestionContent {
  /** The suggestion text. Always provided. */
  suggestion: string;
}

/** Credential selection dialog. */

/** A credential used for the auto-login. */
export declare interface Credential {
  // A unique identifier for this credential. Should not be displayed to the
  // user.
  id: number;
  // The username of the credential. Unique for a given sourceSiteOrApp. It can
  // be empty if, for example, the credential is stored as a password only.
  username: string;
  // The original website or application for which this credential was saved
  // for.
  sourceSiteOrApp: string;
  // The origin for which this credential was requested.
  requestOrigin?: string;
  // The optional icon for the credential, encoded as a PNG image.
  getIcon?(): Promise<Blob>;
}

export declare interface SelectCredentialDialogRequest {
  // The task ID that is requesting the credential selection.
  taskId: number;
  // Whether the web client should show a dialog to let the user select a
  // credential. The web client doesn't have to show the dialog if the user has
  // granted UserGrantedPermissionDuration.ALWAYS_ALLOW to the actor.
  showDialog: boolean;
  // The order of `credentials` is based on what the browser believes to be the
  // best match to use.
  credentials: Credential[];

  // The WebClient must call this function to respond back to the browser when
  // the dialog is closed.
  onDialogClosed(result: {response: SelectCredentialDialogResponse}): void;
}

export declare interface SelectCredentialDialogResponse {
  // The response is associated with the request that has the same task ID.
  taskId: number;
  // Only set if the user changes the permission duration.
  permissionDuration?: UserGrantedPermissionDuration;
  // The ID of the selected credential. Only undefined if the user closed the UI
  // without making a selection.
  selectedCredentialId?: number;
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
  onDialogClosed(result: {response: UserConfirmationDialogResponse}): void;
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
  onConfirmationDecision(result: {response: NavigationConfirmationResponse}):
      void;
}

export declare interface NavigationConfirmationResponse {
  // The verdict of the model if the actor can navigate to this origin.
  permissionGranted?: boolean;
}

/** Autofill suggestion selection dialog. */

/** A single autofill suggestion for a form. */
export declare interface AutofillSuggestion {
  // A unique identifier for this suggestion. Should not be displayed to the
  // user. This string is generated by Autofill for the duration of the
  // suggestions dialog request, which Autofill internally uses to maps to a
  // payload that can be filled.
  id: string;
  // The primary label of the suggestion shown to the user.
  title: string;
  // A secondary label shown below the title shown to the user.
  // Autofill will create this string for display by, possibly, combining
  // other (not exposed) properties of the suggestion.
  details: string;
  // The optional icon for the suggestion, encoded as a PNG image.
  getIcon?(): Promise<Blob>;
}

/**
 * A request to fill a form includes the requested data type and available
 * options.
 */
export declare interface FormFillingRequest {
  // The specific purpose of the form. For example for forms of address type:
  // BILLING_ADDRESS, SHIPPING_ADDRESS, etc.
  // See the FormFillingRequest.RequestedData enum in actions_data.proto.
  requestedData: number;
  // The list of suggestions for this form. The web client shows a selector with
  // these suggestions.
  suggestions: AutofillSuggestion[];
}

/**
 * A request for the web client to show suggestion selectors for a number of
 * forms.
 */
export declare interface SelectAutofillSuggestionsDialogRequest {
  // The list of requested forms to be filled.
  //
  // For example a shipping address with a list of address suggestions and a
  // credit card with another list of suggestions. The web client should show
  // two selectors.
  formFillingRequests: FormFillingRequest[];

  // The WebClient must call this function to respond back to the browser when
  // the dialog is closed.
  onDialogClosed(result: {response: SelectAutofillSuggestionsDialogResponse}):
      void;
}

/**
 * The chosen suggestion from the web client for a single form.
 */
export declare interface FormFillingResponse {
  // The ID corresponding to the user selected suggestion.
  selectedSuggestionId: string;
}

/**
 * The response from the web client containing the chosen suggestions for each
 * form.
 */
export declare interface SelectAutofillSuggestionsDialogResponse {
  // The IDs of the selected suggestions. The order of IDs in this list
  // corresponds to the order of `requests` in the
  // `SelectAutofillSuggestionsDialogRequest`.
  selectedSuggestions: FormFillingResponse[];
}

//
// Types used in presubmit check.
//

// Types to be checked for backwards compatibility on presubmit, excluding
// enums.
export interface BackwardsCompatibleTypes {
  actInFocusedTabParams: ActInFocusedTabParams;
  actInFocusedTabResult: ActInFocusedTabResult;
  additionalContext: AdditionalContext;
  additionalContextPart: AdditionalContextPart;
  annotatedPageData: AnnotatedPageData;
  autofillSuggestion: AutofillSuggestion;
  browserHost: GlicBrowserHost;
  chromeVersion: ChromeVersion;
  createTabOptions: CreateTabOptions;
  credential: Credential;
  documentData: DocumentData;
  draggableArea: DraggableArea;
  focusedTabData: FocusedTabData;
  formFillingRequest: FormFillingRequest;
  glicBrowserHostJournal: GlicBrowserHostJournal;
  glicBrowserHostMetrics: GlicBrowserHostMetrics;
  hostRegistry: GlicHostRegistry;
  imageOriginAnnotations: ImageOriginAnnotations;
  navigationConfirmationRequest: NavigationConfirmationRequest;
  navigationConfirmationResponse: NavigationConfirmationResponse;
  openPanelInfo: OpenPanelInfo;
  panelOpeningData: PanelOpeningData;
  panelState: PanelState;
  pdfDocumentData: PdfDocumentData;
  resizeWindowOptions: ResizeWindowOptions;
  selectAutofillSuggestionsDialogRequest:
      SelectAutofillSuggestionsDialogRequest;
  selectAutofillSuggestionsDialogResponse:
      SelectAutofillSuggestionsDialogResponse;
  selectCredentialDialogRequest: SelectCredentialDialogRequest;
  selectCredentialDialogResponse: SelectCredentialDialogResponse;
  screenshot: Screenshot;
  scrollToParams: ScrollToParams;
  scrollToSelector: ScrollToSelector;
  scrollToTextFragmentSelector: ScrollToTextFragmentSelector;
  scrollToTextSelector: ScrollToTextSelector;
  subscriber: Subscriber;
  tabContextOptions: TabContextOptions;
  tabContextResult: TabContextResult;
  resumeActorTaskResult: ResumeActorTaskResult;
  tabData: TabData;
  userConfirmationDialogRequest: UserConfirmationDialogRequest;
  userConfirmationDialogResponse: UserConfirmationDialogResponse;
  userProfileInfo: UserProfileInfo;
  webClient: GlicWebClient;
  webPageData: WebPageData;
  rect: Rect;
  captureRegionResult: CaptureRegionResult;
  capturedRegion: CapturedRegion;
  openSettingsOptions: OpenSettingsOptions;
  osPermissionType: OsPermissionType;
  zeroStateSuggestions: ZeroStateSuggestions;
  zeroStateSuggestionsV2: ZeroStateSuggestionsV2;
  zeroStateSuggestionsOptions: ZeroStateSuggestionsOptions;
}

// Enums that should not be changed.
export interface ClosedEnums {
  panelStateKind: typeof PanelStateKind;
  webClientMode: typeof WebClientMode;
}

// Enums that can be extended.
export interface ExtensibleEnums {
  captureScreenshotErrorReason: typeof CaptureScreenshotErrorReason;
  captureRegionErrorReason: typeof CaptureRegionErrorReason;
  scrollToErrorReason: typeof ScrollToErrorReason;
  webClientInitializeErrorReason: typeof WebClientInitializeErrorReason;
  invocationSource: typeof InvocationSource;
  actInFocusedTabErrorReason: typeof ActInFocusedTabErrorReason;
  createTaskErrorReason: typeof CreateTaskErrorReason;
  performActionsErrorReason: typeof PerformActionsErrorReason;
  settingsPageField: typeof SettingsPageField;
  hostCapability: typeof HostCapability;
  actorTaskState: typeof ActorTaskState;
  actorTaskPauseReason: typeof ActorTaskPauseReason;
  actorTaskStopReason: typeof ActorTaskStopReason;
  UserGrantedPermissionDuration: typeof UserGrantedPermissionDuration;
  webUseCounter: typeof WebUseCounter;
  platform: typeof Platform;
}

///////////////////////////////////////////////////////////////////////////////
/// BEGIN_GENERATED - DO NOT MODIFY BELOW

// This block is generated by
// chrome/browser/resources/glic/glic_api_impl/generate.py


///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// The type of user input reaction.
export enum MetricUserInputReactionType {
  // An unknown reaction type.
  UNKNOWN = 0,
  // A canned reaction which can be presented without communication with the
  // server.
  CANNED = 1,
  // A reaction which requires some generic modeling to produce.
  MODEL = 2,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Reason for failure while acting.
export enum PerformActionsErrorReason {
  UNKNOWN = 0,
  // The serialized actions proto failed to parse.
  INVALID_ACTION_PROTO = 1,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Reason for failure when creating an actor task.
export enum CreateTaskErrorReason {
  UNKNOWN = 0,
  // The host does not support the actor task system.
  TASK_SYSTEM_UNAVAILABLE = 1,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// The state of an actor task.
export enum ActorTaskState {
  UNKNOWN = 0,
  // The actor task is idle and waiting for the next action instruction.
  IDLE = 1,
  // The actor task is performing an action.
  ACTING = 2,
  // The actor task is paused and waiting to be resumed or stopped.
  PAUSED = 3,
  // The actor task is stopped and going away.
  STOPPED = 4,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// The reason/source of why an actor task was paused.
export enum ActorTaskPauseReason {
  // Actor task was paused by the model.
  PAUSED_BY_MODEL = 0,
  // Actor task was puased by the user.
  PAUSED_BY_USER = 1,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// The reason/source of why an actor task was stopped.
export enum ActorTaskStopReason {
  // Actor task is complete.
  TASK_COMPLETE = 0,
  // Actor task was stopped by the user.
  STOPPED_BY_USER = 1,
  // Actor task was stopped because the model reported a failure.
  MODEL_ERROR = 2,
  // Actor task was stopped by choosing a new conversation.
  USER_STARTED_NEW_CHAT = 3,
  // Actor task was stopped by choosing a previous conversation.
  USER_LOADED_PREVIOUS_CHAT = 4,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Reason why capturing desktop screenshot failed. NOTE: This may be extended in
// the future so avoid using complete switches on the currently used enum
// values.
export enum CaptureScreenshotErrorReason {
  // Screen capture or frame encoding failure.
  UNKNOWN = 0,
  // Screen capture requested but already in progress of serving another
  // request.
  SCREEN_CAPTURE_REQUEST_THROTTLED = 1,
  // User declined screen capture dialog before taking a screenshot.
  USER_CANCELLED_SCREEN_PICKER_DIALOG = 2,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// The platform glic is running on.
export enum Platform {
  UNKNOWN = 0,
  MAC_OS = 1,
  WINDOWS = 2,
  LINUX = 3,
  CHROME_OS = 4,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Reason why scrollTo() failed.
export enum ScrollToErrorReason {
  // Invalid params were provided to scrollTo(), or the browser doesn't support
  // scrollTo() yet.
  NOT_SUPPORTED = 0,
  // scrollTo() was called again before this call finished processing.
  NEWER_SCROLL_TO_CALL = 1,
  // There is no tab currently in focus.
  NO_FOCUSED_TAB = 2,
  // The input selector did not match any content in the document or a given
  // range.
  NO_MATCH_FOUND = 3,
  // The currently focused tab changed or navigated while processing the
  // scrollTo() call.
  FOCUSED_TAB_CHANGED_OR_NAVIGATED = 4,
  // The document_id or url provided doesn't match the active document in the
  // primary main frame of the currently focused tab. The document may have been
  // navigated away, may not currently be in focus, or may not be in a primary
  // main frame (we don't currently support iframes).
  NO_MATCHING_DOCUMENT = 5,
  // The search range starting from DOMNodeId did not result in a valid range.
  SEARCH_RANGE_INVALID = 6,
  // Page context access is disabled.
  TAB_CONTEXT_PERMISSION_DISABLED = 7,
  // The web client requested to drop the highlight via
  // `dropScrollToHighlight()`.
  DROPPED_BY_WEB_CLIENT = 8,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Reason for failure when switching a conversation.
export enum SwitchConversationErrorReason {
  UNKNOWN = 0,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Reason for failure when registering a conversation.
export enum RegisterConversationErrorReason {
  UNKNOWN = 0,
  // The instance already has a conversation ID.
  INSTANCE_ALREADY_HAS_CONVERSATION_ID = 1,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// The panel can be in one of these three states.
export enum PanelStateKind {
  // The panel is hidden.
  HIDDEN = 0,
  // The panel is a floating window, detached from any browser window.
  DETACHED = 1,
  // The panel is a side panel, attached to a browser window.
  ATTACHED = 2,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Entry points that can trigger the opening of the panel.
export enum InvocationSource {
  // Button in the OS.
  OS_BUTTON = 0,
  // Menu from button in the OS.
  OS_BUTTON_MENU = 1,
  // OS-level hotkey.
  OS_HOTKEY = 2,
  // Button in top-chrome.
  TOP_CHROME_BUTTON = 3,
  // First run experience.
  FRE = 4,
  // From the profile picker.
  PROFILE_PICKER = 5,
  // From tab strip nudge.
  NUDGE = 6,
  // From 3-dot menu.
  THREE_DOTS_MENU = 7,
  // An unsupported/unknown source.
  UNSUPPORTED = 8,
  // From the What's New page.
  WHATS_NEW = 9,
  // User clicked the sign-in button and signed in.
  AFTER_SIGN_IN = 10,
  // User shared a tab (e.g. via its context menu).
  SHARED_TAB = 11,
  // From the actor task icon.
  ACTOR_TASK_ICON = 12,
  // User shared an image via the context menu.
  SHARED_IMAGE = 13,
  // From the handoff button.
  HANDOFF_BUTTON = 14,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Web client's operation modes.
export enum WebClientMode {
  // Text operation mode.
  TEXT = 0,
  // Audio operation mode.
  AUDIO = 1,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Web client's operation model.
export enum WebClientModel {
  // Default model.
  DEFAULT = 0,
  // Actor operation mode.
  ACTOR = 1,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Web client's user counter.
export enum WebUseCounter {
  // Default counter. Never used.
  DEFAULT = 0,
  SUBMIT_PROMPT_WITH_AUTO_MODE = 1,
  TASK_INTERRUPTED_FOR_USER_CONFIRMATION = 2,
  TASK_INTERRUPTED_FOR_USER_CLARIFICATION = 3,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Reason for `captureRegion` error.
export enum CaptureRegionErrorReason {
  UNKNOWN = 0,
  // There is no focused tab that can be used for region capture.
  NO_FOCUSABLE_TAB = 1,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Fields of interest from the Glic settings page.
export enum SettingsPageField {
  // The OS hotkey configuration field.
  OS_HOTKEY = 1,
  // The OS entrypoint enabling field.
  OS_ENTRYPOINT_TOGGLE = 2,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Describes the capability of the glic host.
export enum HostCapability {
  // Glic host supports scrollTo() with PDF documents.
  SCROLL_TO_PDF = 0,
  // Glic host will reset panel size and location on open.
  RESET_SIZE_AND_LOCATION_ON_OPEN = 1,
  // The glic host's getModelQualityClientId() is enabled and can be called
  // safely.
  GET_MODEL_QUALITY_CLIENT_ID = 2,
  // Glic is in multi-instance mode.
  MULTI_INSTANCE = 3,
  // Enables the experimental "Trust First" (Arm 1 - "Start Chat") onboarding
  // UI flow, bypassing the standard FRE flow.
  TRUST_FIRST_ONBOARDING_ARM1 = 4,
  // Enables the experimental "Trust First" (Arm 2 - "Welcome Screen")
  // onboarding UI flow, bypassing the standard FRE flow.
  TRUST_FIRST_ONBOARDING_ARM2 = 5,
}

///////////////////////////////////////////////
// WARNING - GENERATED FROM MOJOM, DO NOT EDIT.
// Describes how long the user grants the actor with the permission to actuate.
// Used when the actor is to actuate with sensitive data, such as entering
// payment information or login credentials.
export enum UserGrantedPermissionDuration {
  // The user only grants a one-time permission. The user will be asked again.
  // This is the default behavior.
  ONE_TIME = 0,
  // The user grants a permission to always allow the actor to actuate with
  // sensitive data. The persistence of this permission is defined differently
  // for different features.
  ALWAYS_ALLOW = 1,
}


/// END_GENERATED - DO NOT MODIFY ABOVE
///////////////////////////////////////////////////////////////////////////////
