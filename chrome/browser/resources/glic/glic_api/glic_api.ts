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
   * @todo Remove void promise value once the web client returns OpenPanelInfo.
   *       https://crbug.com/391946150
   *
   * @todo The browser is currently storing the previous panel size, but the web
   *       client should be updated to set the panel size when handling this
   *       call. https://crbug.com/392141194
   *
   * Called right before the panel is made visible to the user. This event is
   * always called no matter how the panel opening is initiated.
   *
   * The web client should use the handling of this call to execute any
   * preparations needed to become user-visible, and return a fully populated
   * OpenPanelInfo.
   *
   * Important: The panel is only made user-visible once the returned promise is
   * resolved or failed (failures are ignored and the panel is still shown).
   */
  notifyPanelWillOpen?(panelState: PanelState): Promise<void|OpenPanelInfo>;

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
   * The user has requested activation of the web client.
   * The attachedToWindowId identifies the browser window to which the
   * panel is attached to. It is undefined if it is detached.
   *
   * Note: The returned promise is currently not used in the browser.
   *
   * @deprecated: Not supported anymore and will eventually be removed.
   */
  notifyPanelOpened?(attachedToWindowId: string|undefined): Promise<void>;

  /**
   * The user has closed the web client window. The window may be activated
   * again later.
   *
   * The promise being resolved indicates the web client has stored any needed
   * information and stopped accepting the user's input.
   *
   * @deprecated: Not supported anymore and will eventually be removed.
   */
  notifyPanelClosed?(): Promise<void>;
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
   * Set the areas of the glic window from which it should be draggable. If
   * `areas` is empty, a default draggable area will be created.
   *
   * Returns a promise that resolves when the browser has updated the draggable
   * area.
   */
  setWindowDraggableAreas(areas: DraggableArea[]): Promise<void>;

  /**
   * Fetches page context for the currently focused tab, optionally including
   * more expensive-to-generate data.
   *
   * @throws {GetTabContextError} on failure.
   */
  getContextFromFocusedTab?
      (options: TabContextOptions): Promise<TabContextResult>;

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

  /** Opens a new tab to the glic settings page. */
  openGlicSettingsPage?(): void;

  /** Requests the closing of the panel containing the web client. */
  closePanel?(): Promise<void>;

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

  /** Returns the state of the panel. */
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
   * Whether the panel can be attached. This is true if there is a browser
   * window suitable for attachment. This state is only meaningful when the
   * panel is in the detached state, and should be not be considered otherwise
   * (i.e. it will not necessarily become false when the panel becomes
   * attached). When this is false, calls to attachPanel() are likely to fail.
   */
  canAttachPanel?(): ObservableValue<boolean>;

  /**
   * Returns the observable state of the currently focused tab. Updates are sent
   * whenever the focus changes due to the user switching tabs or navigating the
   * current focused tab.
   *
   * @returns An ObservableValue for `TabData` values that will be updated when
   *          a new tab is focused or the current tab is navigated. The value
   *          will be `undefined` if there's no active tab or it cannot be
   *          focused (i.e. the URL is ineligible for tab context sharing).
   *
   * @deprecated Use `getFocusedTabStateV2` instead. This function returns a
   * TabData on success but no information at all on failure. V2 solves this by
   * returning error codes to signal why no focus was available.
   */
  getFocusedTabState?(): ObservableValue<TabData|undefined>;

  /**
   * Returns the observable state of the currently focused tab. Updates are sent
   * whenever:
   * - the user switches active tabs
   * - the active tab navigates to a new url
   * - tab focus changes or is lost
   * - any field of TabData needs to be updated to match the current tab
   */
  getFocusedTabStateV2?(): ObservableValue<FocusedTabData>;

  /** Returns the state of the microphone permission. */
  getMicrophonePermissionState?(): ObservableValue<boolean>;

  /** Returns the state of the location permission. */
  getLocationPermissionState?(): ObservableValue<boolean>;

  /** Returns the state of the tab context permission. */
  getTabContextPermissionState?(): ObservableValue<boolean>;

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
   * selector. Returns a promise that resolves when the selected content is
   * matched and a scroll is started.
   *
   * @throws {ScrollToError} on failure.
   */
  scrollTo?(params: ScrollToParams): Promise<void>;

  /**
   * Enrolls the Chrome client in the synthetic experiment group specified by
   * trial_name.group_name. Enrollment will only start when the API is called
   * and end when Chrome closes.
   */
  setSyntheticExperimentState?(trialName: string, groupName: string): void;
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
 * @todo Not yet implemented. https://crbug.com/391417447
 *
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
}

/** A panel can be in one of these three states. */
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

/** Information of how the panel is being presented/configured. */
export declare interface PanelState {
  /** The panel's presentation kind/state. */
  kind: PanelStateKind;
  /**
   * Present only when attached to a window, indicating which window it is
   * attached to.
   */
  windowId?: string;
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
}

/**
 * Various bits of data about a browser tab. Optional fields may not be
 * available while the page is being loaded or if not provided by the page
 * itself.
 */
export declare interface TabData {
  /** Unique ID of the tab that owns the page. */
  tabId: string;
  /** Unique ID of the browser window holding the tab. */
  windowId: string;
  /** URL of the page. */
  url: string;
  /**
   * The title of the loaded page. Returned only if the page is loaded enough
   * for it to be available. It may be empty if the page did not define a title.
   */
  title?: string;
  /**
   * Returns the favicon for the tab, encoded as a PNG image. Returned only if
   * the page is loaded enough for it to be available and the page specifies
   * one.
   */
  favicon?(): Promise<Blob|undefined>;
  /**
   * MIME type of the main document. Returned only if the page is loaded enough
   * for it to be available.
   */
  documentMimeType?: string;
}

/** Data class holding information about the focused tab state. */
export declare interface FocusedTabData {
  /** Stores the focused tab data if one exists. */
  focusedTab?: TabData;
  /**
   * If a focus candidate exists but cannot be focused then
   * `focusedTabCandidate` will hold its `TabData` and an
   * `InvalidCandidateError` specifying why it is not focusable.
   */
  focusedTabCandidate?: FocusedTabCandidate;
  /** If no candidate exists than the noCandidateTabError will indicate why. */
  noCandidateTabError?: NoCandidateTabError;
}

/** Data class holding information about the focused tab candidate. */
export declare interface FocusedTabCandidate {
  /**
   * Stores the focused tab candidate data if the browser has valid TabData
   * which cannot be used for context extraction.
   */
  focusedTabCandidateData?: TabData;
  /** Specifies why the candidate was invalid for focus. */
  invalidCandidateError?: InvalidCandidateError;
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
  tabContext: GetTabContextErrorReason;
  captureScreenshot: CaptureScreenshotErrorReason;
  scrollTo: ScrollToErrorReason;
  webClientInitialize: WebClientInitializeErrorReason;
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

/** Reason for failure while extracting tab context. */
export enum GetTabContextErrorReason {
  UNKNOWN = 0,
  /** The web contents was navigated or closed during context gathering. */
  WEB_CONTENTS_CHANGED = 1,
  /** Permission to capture page context is denied. */
  PERMISSION_DENIED = 2,
  /** The URL in the tab data is not supported. */
  UNSUPPORTED_URL = 3,
  /** There are no Chrome tabs available to be focused. */
  NO_FOCUSABLE_TABS = 4,
}

/**
 * Reason why a focused tab candidate is not valid for focus. NOTE: This may be
 * extended in the future so avoid using complete switches on the currently used
 * enum values.
 */
export enum InvalidCandidateError {
  /** Candidate invalid for an unknown reason. */
  UNKNOWN = 0,
  /** The URL in the tab data is not supported. */
  UNSUPPORTED_URL = 1,
}

/**
 * Reason why a focused tab is not available. NOTE: This may be extended in the
 * future so avoid using complete switches on the currently used enum values.
 */
export enum NoCandidateTabError {
  /** An unknown error occurred while getting the tab data. */
  UNKNOWN = 0,
  /** There are no Chrome tabs available to be focused. */
  NO_FOCUSABLE_TABS = 1,
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

/** Error type used for tab context extraction errors. */
export type GetTabContextError = ErrorWithReason<'tabContext'>;

/** Error type used for screenshot capture errors. */
export type CaptureScreenshotError = ErrorWithReason<'captureScreenshot'>;

/** Params for scrollTo(). */
export declare interface ScrollToParams {
  /**
   * Whether we should highlight the content selected. True by default if not
   * specified. If false, the content is scrolled to but not highlighted.
   */
  highlight?: boolean;

  /** Used to specify content to scroll to and highlight. */
  selector: ScrollToSelector;
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
   * Text fragment selector, see ScrollToTextFragmentSelector for more details
   */
  textFragment?: ScrollToTextFragmentSelector;
}

/**
 * scrollTo() selector to select exact text in HTML and PDF documents.
 */
export declare interface ScrollToTextSelector {
  text: string;
}

/**
 * scrollTo() selector to select a range of text in HTML and PDF documents.
 * Text selected will match textStart <anything in the middle> textEnd.
 */
export declare interface ScrollToTextFragmentSelector {
  textStart: string;
  textEnd: string;
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
  /** Whether the profile or the browser is managed. */
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

//
// Types used in presubmit check.
//

// Types consumed by the client. These are subject to stricter checks than
// those in TypesConsumedByHost.
export interface TypesConsumedByClient {
  hostRegistry: GlicHostRegistry;
  browserHost: GlicBrowserHost;
  tabContextResult: TabContextResult;
  tabData: TabData;
  imageOriginAnnotations: ImageOriginAnnotations;
  screenshot: Screenshot;
  userProfileInfo: UserProfileInfo;
  chromeVersion: ChromeVersion;
  focusedTabData: FocusedTabData;
  focusedTabCandidate: FocusedTabCandidate;
  pdfDocumentData: PdfDocumentData;
  webPageData: WebPageData;
  documentData: DocumentData;
  panelState: PanelState;
  annotatedPageData: AnnotatedPageData;
}

// Types consumed by the host.
export interface TypesConsumedByHost {
  webClient: GlicWebClient;
  resizeWindowOptions: ResizeWindowOptions;
  createTabOptions: CreateTabOptions;
  glicBrowserHostMetrics: GlicBrowserHostMetrics;
  openPanelInfo: OpenPanelInfo;
  tabContextOptions: TabContextOptions;
  draggableArea: DraggableArea;
  subscriber: Subscriber;
  scrollToParams: ScrollToParams;
  scrollToSelector: ScrollToSelector;
  scrollToTextSelector: ScrollToTextSelector;
  scrollToTextFragmentSelector: ScrollToTextFragmentSelector;
}

// Enums that should not be changed.
export interface ClosedEnums {
  panelStateKind: typeof PanelStateKind;
  webClientMode: typeof WebClientMode;
}

// Enums that can be extended.
export interface ExtensibleEnums {
  getTabContextErrorReason: typeof GetTabContextErrorReason;
  captureScreenshotErrorReason: typeof CaptureScreenshotErrorReason;
  scrollToErrorReason: typeof ScrollToErrorReason;
  invalidCandidateError: typeof InvalidCandidateError;
  noCandidateTabError: typeof NoCandidateTabError;
  webClientInitializeErrorReason: typeof WebClientInitializeErrorReason;
}
