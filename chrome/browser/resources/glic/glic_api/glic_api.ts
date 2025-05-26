// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API between the Chrome browser and the Glic web client.
//
// Overall notes:
// - There will only ever be one single instance of the web client running at
//   a time. It may be destroyed and restarted, and each time the initialization
//   process will be repeated.
// - As in TypeScript all `number`s are 64 bit floating points, we decided to
//   make all identifier values be of the `string` type (e.g. for a window or a
//   tab).
// - The defined functions and interfaces can be "evolved" to provide more
//   functionality and data, as needed.
// - Functions are documented with their known behavior. Exceptions and promise
//   failures should will be documented only if they are expected.
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
}

/**
 * Provides functionality implemented by the browser to the Glic web client.
 * Most functions are optional.
 */
export declare interface GlicBrowserHost {
  /** Returns the precise Chrome's version. */
  getChromeVersion(): Promise<ChromeVersion>;

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
   * Returns true if the web client should resize its content to fit the
   * window.
   *
   * @todo This should be the default sizing mode. Remove after the manual
   * resizing is landed. crbug.com/402795394.
   */
  shouldFitWindow?(): Promise<boolean>;

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
   * @todo Not yet implemented. https://crbug.com/402086021
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
   * Stops the actor task with the given ID in the browser if it exists. No-op
   * otherwise.
   *
   * Stopping a task removes all actor related restrictions from the associated
   * tab. Any in progress actions are canceled and the associated Promises are
   * rejected.
   *
   * If the task ID is not provided or 0, the most recent task is stopped.
   *
   * @todo Require callers to provide a valid ID.
   */
  stopActorTask?(taskId?: number): void;

  /**
   * Pauses the actor task with the given ID in the browser if it exists. No-op
   * otherwise.
   *
   * Pausing a task removes actor related restrictions that prevent the user
   * from interacting with the associated tab. Any in progress actions are
   * canceled and the associated Promises are rejected.
   *
   * If the task ID is 0, the most recent task is paused.
   *
   * @todo Require callers to provide a valid ID.
   */
  pauseActorTask?(taskId: number): void;

  /**
   * Resumes a previously paused actor task with the given ID.
   *
   * Returns the tab context at the time of resumption, based on the provided
   * context options.
   *
   * If the task ID is 0, the most recent task is resumed.
   *
   * @throws {Error} on failure.
   *
   * @todo Require callers to provide a valid ID.
   */
  resumeActorTask?(taskId: number, tabContextOptions: TabContextOptions):
      Promise<TabContextResult>;

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
   * @deprecated The panel will only maintain the detached state.
   *
   * Requests that the web client's panel be attached to a browser window.
   * If attachment fails, the panel's state will not be updated. getPanelState
   * can be used to monitor whether attachment is successful.
   */
  attachPanel?(): void;

  /**
   * @deprecated The panel will only maintain the detached state.
   *
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
   * @deprecated The panel will only maintain the detached state.
   *
   * Returns the state of the panel.
   */
  getPanelState?(): ObservableValue<PanelState>;

  /**
   * Whether the glic panel is in the active state. In the inactive state,
   * microphone recording should stop, but any playing audio output can
   * continue.
   *
   * Note that the Glic panel is inactive if it is attached to a browser window,
   * and that browser window is not the current active window.
   */
  panelActive(): ObservableValue<boolean>;

  /**
   * @deprecated The panel will only maintain the detached state.
   *
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
   * @deprecated Use `getFocusedTabStateV2` instead. This function returns a
   * TabData on success but no information at all on failure. V2 solves this by
   * returning error codes to signal why no focus was available.
   *
   * Returns the observable state of the currently focused tab. Updates are sent
   * whenever the focus changes due to the user switching tabs or navigating the
   * current focused tab.
   *
   * @returns An ObservableValue for `TabData` values that will be updated when
   *          a new tab is focused or the current tab is navigated. The value
   *          will be `undefined` if there's no active tab or it cannot be
   *          focused (i.e. the URL is ineligible for tab context sharing).
   */
  getFocusedTabState?(): ObservableValue<TabData|undefined>;

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
   * @todo Not yet implemented. https://crbug.com/404617216
   *
   * Returns the set of zero state suggestions for the currently focused tab
   * based on if the client is currently in it's is_first_run.
   * Callers should verify the current focused tab matches the
   * ZeroStateSuggestions tabId and url before using it.
   */
  getZeroStateSuggestionsForFocusedTab?
      (is_first_run?: boolean): Promise<ZeroStateSuggestions>;
}
/** Fields of interest from the system settings page. */
export type OsPermissionType = 'media'|'geolocation';

/** Fields of interest from the Glic settings page. */
export enum SettingsPageField {
  /** The OS hotkey configuration field. */
  OS_HOTKEY = 1,
  /** The OS entrypoint enabling field. */
  OS_ENTRYPOINT_TOGGLE = 2,
}

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

/**
 * Provides measurement-related functionality to the Glic web client.
 *
 * The typical sequence of events should be either:
 *  onUserInputSubmitted -> onResponseStarted -> onResponseStopped -> (repeat)
 * or
 *  onUserInputSubmitted -> onResponseStopped -> (repeat)
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
   * Called when the web client has sufficiently processed the input such that
   * it is able to start providing a response.
   */
  onResponseStarted?(): void;

  /**
   * Called when the response was completed, cancelled, or paused for the first
   * time.
   */
  onResponseStopped?(): void;

  /** Called when a session terminates. */
  onSessionTerminated?(): void;

  /** Called when the user rates a response. */
  onResponseRated?(positive: boolean): void;
}

/** Web client's operation modes */
export enum WebClientMode {
  /** Text operation mode. */
  TEXT = 0,
  /** Audio operation mode. */
  AUDIO = 1,
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
 * @deprecated The panel will only maintain the detached state.
 *
 * A panel can be in one of these three states.
 */
export enum PanelStateKind {
  /** Not shown. This is the initial state. */
  HIDDEN = 0,
  /** @deprecated Use DETACHED instead. */
  FLOATING = 1,
  /** A floating window detached from any Chrome window. */
  DETACHED = 1,
  /** @deprecated Use ATTACHED instead.*/
  DOCKED = 2,
  /** Attached to a Chrome window. */
  ATTACHED = 2,
}

/**
 * @deprecated The panel will only maintain the detached state.
 *
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
   * @deprecated The panel will only maintain the detached state.
   *
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
}

/** Entry points that can trigger the opening of the panel. */
export enum InvocationSource {
  /** Button in the OS. */
  OS_BUTTON = 0,
  /** Menu from button in the OS. */
  OS_BUTTON_MENU = 1,
  /** OS-level hotkey. */
  OS_HOTKEY = 2,
  /** Button in top-chrome. */
  TOP_CHROME_BUTTON = 3,
  /** First run experience. */
  FRE = 4,
  /** From the profile picker. */
  PROFILE_PICKER = 5,
  /** From contextual cueing. */
  NUDGE = 6,
  /** From 3-dot menu. */
  THREE_DOTS_MENU = 7,
  /** An unsupported/unknown source. */
  UNSUPPORTED = 8,
  /** From the What's New page. */
  WHATS_NEW = 9,
  /** User clicks sign-in and then signs in. */
  AFTER_SIGN_IN = 10,
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
   * MIME type of the main document. Returned only if the page is loaded enough
   * for it to be available.
   */
  documentMimeType?: string;
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
 *
 * Note: Only JPEG images will be supported initially, so mimeType will always
 * be "image/jpeg".
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

/** Maps the ErrorWithReason.reasonType to the type of reason. */
export declare interface ErrorReasonTypes {
  captureScreenshot: CaptureScreenshotErrorReason;
  scrollTo: ScrollToErrorReason;
  webClientInitialize: WebClientInitializeErrorReason;
  actInFocusedTab: ActInFocusedTabErrorReason;
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
}

/**
 * Reason why capturing desktop screenshot failed. NOTE: This may be extended in
 * the future so avoid using complete switches on the currently used enum
 * values.
 */
export enum CaptureScreenshotErrorReason {
  /** Screen capture or frame encoding failure. */
  UNKNOWN = 0,
  /**
   * Screen capture requested but already in progress of serving another
   * request.
   */
  SCREEN_CAPTURE_REQUEST_THROTTLED = 1,
  /** User declined screen capture dialog before taking a screenshot. */
  USER_CANCELLED_SCREEN_PICKER_DIALOG = 2,
}

export declare interface ActInFocusedTabResult {
  // The tab context result after acting and gathering new context.
  tabContextResult?: TabContextResult;
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

/** Error type used for actuation errors. */
export type ActInFocusedTabError = ErrorWithReason<'actInFocusedTab'>;

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
   * ID, and throw an error if doesn't. If not specified, the implementation
   * will use the main frame of the currently focused tab without verification.
   *
   * Note: documentId is being migrated to become a required param and the
   * client will soon throw a NotSupported error (behind a flag currently) when
   * not specified.
   */
  documentId?: string;
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

/** Reason why scrollTo() failed. */
export enum ScrollToErrorReason {
  /**
   * Invalid params were provided to scrollTo(), or the browser doesn't support
   * scrollTo() yet.
   */
  NOT_SUPPORTED = 0,
  /** scrollTo() was called again before this call finished processing. */
  NEWER_SCROLL_TO_CALL = 1,
  /** There is no tab currently in focus. */
  NO_FOCUSED_TAB = 2,
  /** The selector did not match any content in the document. */
  NO_MATCH_FOUND = 3,
  /**
   * The currently focused tab changed or navigated while processing the
   * scrollTo() call.
   */
  FOCUSED_TAB_CHANGED_OR_NAVIGATED = 4,
  /**
   * The documentId provided doesn't match the currently focused tab's primary
   * document. The document may have been navigated away, may not currently be
   * in focus, or may not be in a primary main frame (we don't currently support
   * iframes).
   */
  NO_MATCHING_DOCUMENT = 5,

  /**
   *  The search range starting from DOMNodeId did not result in a valid range.
   */
  SEARCH_RANGE_INVALID = 6,

  /**
   * Tab context permission was disabled.
   */
  TAB_CONTEXT_PERMISSION_DISABLED = 7,

  /**
   * The web client requested to drop the highlight via `dropScrollToHighlight`.
   */
  DROPPED_BY_WEB_CLIENT = 8,
}

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
 * A generic interface for observing a stream of values.
 *
 * Subscriptions should be kept only while necessary, as they incur some cost.
 * When not needed anymore, call Subscriber.unsubscribe() on the instance
 * returned by subscribe.
 */
export declare interface Observable<T> {
  /** Receive updates for value changes. */
  subscribe(change: (newValue: T) => void): Subscriber;
}

/**
 * An observable value that may change over time. A subscriber is guaranteed to
 * be called once with the value, and again anytime the value changes. Note that
 * the subscriber may or may not be invoked immediately upon calling
 * subscribe().
 *
 * See also comments about Observable.
 */
export interface ObservableValue<T> extends Observable<T> {}

/** Allows control of a subscription to an Observable. */
export declare interface Subscriber {
  unsubscribe(): void;
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

//
// Types used in presubmit check.
//

// Types to be checked for backwards compatibility on presubmit, excluding
// enums.
export interface BackwardsCompatibleTypes {
  actInFocusedTabParams: ActInFocusedTabParams;
  actInFocusedTabResult: ActInFocusedTabResult;
  annotatedPageData: AnnotatedPageData;
  browserHost: GlicBrowserHost;
  chromeVersion: ChromeVersion;
  createTabOptions: CreateTabOptions;
  documentData: DocumentData;
  draggableArea: DraggableArea;
  focusedTabData: FocusedTabData;
  glicBrowserHostMetrics: GlicBrowserHostMetrics;
  hostRegistry: GlicHostRegistry;
  imageOriginAnnotations: ImageOriginAnnotations;
  openPanelInfo: OpenPanelInfo;
  panelOpeningData: PanelOpeningData;
  panelState: PanelState;
  pdfDocumentData: PdfDocumentData;
  resizeWindowOptions: ResizeWindowOptions;
  screenshot: Screenshot;
  scrollToParams: ScrollToParams;
  scrollToSelector: ScrollToSelector;
  scrollToTextFragmentSelector: ScrollToTextFragmentSelector;
  scrollToTextSelector: ScrollToTextSelector;
  subscriber: Subscriber;
  tabContextOptions: TabContextOptions;
  tabContextResult: TabContextResult;
  tabData: TabData;
  userProfileInfo: UserProfileInfo;
  webClient: GlicWebClient;
  webPageData: WebPageData;
  openSettingsOptions: OpenSettingsOptions;
  osPermissionType: OsPermissionType;
  zeroStateSuggestions: ZeroStateSuggestions;
}

// Enums that should not be changed.
export interface ClosedEnums {
  panelStateKind: typeof PanelStateKind;
  webClientMode: typeof WebClientMode;
}

// Enums that can be extended.
export interface ExtensibleEnums {
  captureScreenshotErrorReason: typeof CaptureScreenshotErrorReason;
  scrollToErrorReason: typeof ScrollToErrorReason;
  webClientInitializeErrorReason: typeof WebClientInitializeErrorReason;
  invocationSource: typeof InvocationSource;
  actInFocusedTabErrorReason: typeof ActInFocusedTabErrorReason;
  settingsPageField: typeof SettingsPageField;
}
