// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in
// glic_api_host/glic_api_impl.ts.

import {assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {BitmapN32} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import {AlphaType} from '//resources/mojo/skia/public/mojom/image_info.mojom-webui.js';
import type {Origin} from '//resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {SelectCredentialDialogErrorReason as SelectCredentialDialogErrorReasonMojo, SelectCredentialDialogRequest as SelectCredentialDialogRequestMojo, SelectCredentialDialogResponse as SelectCredentialDialogResponseMojo, TaskOptions as TaskOptionsMojo, UserConfirmationDialogErrorReason as UserConfirmationDialogErrorReasonMojo, UserConfirmationDialogRequest as UserConfirmationDialogRequestMojo, UserConfirmationDialogResponse as UserConfirmationDialogResponseMojo, UserGrantedPermissionDuration as UserGrantedPermissionDurationMojo} from '../actor_webui.mojom-webui.js';
import type {PageMetadata as PageMetadataMojo} from '../ai_page_content_metadata.mojom-webui.js';
import type {BrowserProxy} from '../browser_proxy.js';
import {ContentSettingsType} from '../content_settings_types.mojom-webui.js';
import type {ActorTaskPauseReason as ActorTaskPauseReasonMojo, ActorTaskState as ActorTaskStateMojo, ActorTaskStopReason as ActorTaskStopReasonMojo, AdditionalContext as AdditionalContextMojo, AnnotatedPageData as AnnotatedPageDataMojo, ContextData as ContextDataMojo, FocusedTabData as FocusedTabDataMojo, GetPinCandidatesOptions as GetPinCandidatesOptionsMojo, GetTabContextOptions as TabContextOptionsMojo, HostCapability as HostCapabilityMojo, OpenPanelInfo as OpenPanelInfoMojo, OpenSettingsOptions as OpenSettingsOptionsMojo, PanelOpeningData as PanelOpeningDataMojo, PanelState as PanelStateMojo, PdfDocumentData as PdfDocumentDataMojo, PinCandidate as PinCandidateMojo, PinCandidatesObserver, Screenshot as ScreenshotMojo, ScrollToSelector as ScrollToSelectorMojo, TabContext as TabContextMojo, TabData as TabDataMojo, ViewChangeRequest as ViewChangeRequestMojo, WebClientHandlerInterface, WebClientInitialState, WebClientInterface, WebPageData as WebPageDataMojo, ZeroStateSuggestionsOptions as ZeroStateSuggestionsOptionsMojo, ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo} from '../glic.mojom-webui.js';
import {CurrentView as CurrentViewMojo, PinCandidatesObserverReceiver, ResponseStopCause as ResponseStopCauseMojo, SettingsPageField as SettingsPageFieldMojo, WebClientHandlerRemote, WebClientMode, WebClientReceiver} from '../glic.mojom-webui.js';
import type {ActorTaskPauseReason, ActorTaskState, ActorTaskStopReason, ConversationInfo, DraggableArea, GetPinCandidatesOptions, HostCapability, Journal, OnResponseStoppedDetails, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, Screenshot, ScrollToParams, TabContextOptions, TaskOptions, ViewChangedNotification, ViewChangeRequest, WebPageData, ZeroStateSuggestions, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../glic_api/glic_api.js';
import {CaptureScreenshotErrorReason, ClientView, CreateTaskErrorReason, DEFAULT_INNER_TEXT_BYTES_LIMIT, DEFAULT_PDF_SIZE_LIMIT, PerformActionsErrorReason, ResponseStopCause, ScrollToErrorReason} from '../glic_api/glic_api.js';
import {ObservableValue} from '../observable.js';
import type {ObservableValueReadOnly} from '../observable.js';
import {OneShotTimer} from '../timer.js';

import {replaceProperties} from './conversions.js';
import type {PostMessageRequestHandler} from './post_message_transport.js';
import {newSenderId, PostMessageRequestReceiver, PostMessageRequestSender, ResponseExtras} from './post_message_transport.js';
import type {AdditionalContextPartPrivate, AdditionalContextPrivate, AllRequestTypesWithoutReturn, AllRequestTypesWithReturn, AnnotatedPageDataPrivate, FocusedTabDataPrivate, HostRequestTypes, PdfDocumentDataPrivate, RequestRequestType, RequestResponseType, RgbaImage, SelectCredentialDialogRequestPrivate, SelectCredentialDialogResponsePrivate, TabContextResultPrivate, TabDataPrivate, TransferableException, UserConfirmationDialogRequestPrivate, UserConfirmationDialogResponsePrivate, WebClientInitialStatePrivate, WebClientRequestTypes} from './request_types.js';
import {ErrorWithReasonImpl, exceptionFromTransferable, HOST_REQUEST_TYPES, ImageAlphaType, ImageColorType, requestTypeToHistogramSuffix} from './request_types.js';

export enum WebClientState {
  UNINITIALIZED,
  RESPONSIVE,
  UNRESPONSIVE,
  ERROR,  // Final state
}

enum PanelOpenState {
  OPEN,
  CLOSED,
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DetailedWebClientState)
export enum DetailedWebClientState {
  BOOTSTRAP_PENDING = 0,
  WEB_CLIENT_NOT_CREATED = 1,
  WEB_CLIENT_INITIALIZE_FAILED = 2,
  WEB_CLIENT_NOT_INITIALIZED = 3,
  TEMPORARY_UNRESPONSIVE = 4,
  PERMANENT_UNRESPONSIVE = 5,
  RESPONSIVE = 6,
  RESPONSIVE_INACTIVE = 7,
  UNRESPONSIVE_INACTIVE = 8,
  MOJO_PIPE_CLOSED_UNEXPECTEDLY = 9,
  MAX_VALUE = MOJO_PIPE_CLOSED_UNEXPECTEDLY,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicDetailedWebClientState)

// Implemented by the embedder of GlicApiHost.
export interface ApiHostEmbedder {
  // Called when the guest requests resize.
  onGuestResizeRequest(size: {width: number, height: number}): void;

  // Called when the guest requests to enable manual drag resize.
  enableDragResize(enabled: boolean): void;

  // Called when the notifyPanelWillOpen promise resolves to open the panel
  // when triggered from the browser.
  webClientReady(): void;
}

// Turn everything except void into a promise.
type Promisify<T> = T extends void ? void : Promise<T>;

// Is a request type allowed in the background?
type IsBackgroundRequest<T extends keyof HostRequestTypes> =
    'backgroundAllowed' extends keyof HostRequestTypes[T] ?
    HostRequestTypes[T]['backgroundAllowed'] :
    false;

// Configuration of how to handle requests received while glic is in the
// background. Requests that are annotated with `backgroundAllowed` are
// unaffected. Otherwise, an entry must exist in `BACKGROUND_RESPONSES`
// to define behavior.
// Note that if glic becomes backgrounded while a request is being processed,
// the request will not be affected.

// Throw an exception, returning an error to the client.
interface HostBackgroundResponseThrows {
  throws: true;
}

// Run `does()` and return its result to the client.
interface HostBackgroundResponseDoes<R> {
  does: () => R;
}

// Returns a constant value to the client.
interface HostBackgroundResponseReturns<R> {
  returns: R;
}

type HostBackgroundResponse<R> = HostBackgroundResponseThrows|
    HostBackgroundResponseReturns<R>|HostBackgroundResponseDoes<R>;

type HostBackgroundResponseMap = {
  [RequestName in keyof HostRequestTypes as
       IsBackgroundRequest<RequestName> extends true ? never : RequestName]:
      HostBackgroundResponse<RequestResponseType<RequestName>>;
};

// How to respond to each requests received in the background. One entry for
// each request type that does not specify `backgroundAllowed`.
const BACKGROUND_RESPONSES: HostBackgroundResponseMap = {
  glicBrowserCreateTab: {returns: {}},
  glicBrowserShowProfilePicker: {throws: true},
  glicBrowserGetContextFromFocusedTab: {throws: true},
  glicBrowserGetContextFromTab: {throws: true},
  glicBrowserCaptureScreenshot: {throws: true},
  glicBrowserScrollTo: {
    does: () => {
      throw new ErrorWithReasonImpl(
          'scrollTo', ScrollToErrorReason.NOT_SUPPORTED);
    },
  },
  glicBrowserOpenOsPermissionSettingsMenu: {throws: true},
  glicBrowserPinTabs: {returns: {pinnedAll: false}},
  glicBrowserUnpinAllTabs: {returns: undefined},
  glicBrowserSubscribeToPinCandidates: {returns: undefined},
  glicBrowserGetZeroStateSuggestionsForFocusedTab: {returns: {}},
  glicBrowserGetZeroStateSuggestionsAndSubscribe: {returns: {}},
};

// A type which the host should implement. This helps verify that
// `HostMessageHandler` is implemented with the correct parameter and return
// types.
type HostMessageHandlerInterface = {
  [Property in keyof HostRequestTypes as string extends Property ? never :
                                                                   Property]:
      // `payload` is the message payload.
  (payload: RequestRequestType<Property>, extras: ResponseExtras) =>
      Promisify<RequestResponseType<Property>>;
};

type IsGatedRequest<T extends keyof WebClientRequestTypes> =
    'backgroundAllowed' extends keyof WebClientRequestTypes[T] ? false : true;
type UngatedWebClientRequestTypes = {
  [Property in keyof WebClientRequestTypes as
       IsGatedRequest<Property> extends true ? never : Property]: true;
};

class WebClientImpl implements WebClientInterface {
  private sender: GatedSender;

  constructor(private host: GlicApiHost, private embedder: ApiHostEmbedder) {
    this.sender = this.host.sender;
  }

  async notifyPanelWillOpen(panelOpeningData: PanelOpeningDataMojo):
      Promise<{openPanelInfo: OpenPanelInfoMojo}> {
    this.host.setWaitingOnPanelWillOpen(true);
    let result;
    try {
      result = await this.sender.requestWithResponse(
          'glicWebClientNotifyPanelWillOpen',
          {panelOpeningData: panelOpeningDataToClient(panelOpeningData)});
    } finally {
      this.host.setWaitingOnPanelWillOpen(false);
      this.host.panelOpenStateChanged(PanelOpenState.OPEN);
    }

    // The web client is ready to show, ensure the webview is
    // displayed.
    this.embedder.webClientReady();

    const openPanelInfoMojo: OpenPanelInfoMojo = {
      webClientMode:
          (result.openPanelInfo?.startingMode as WebClientMode | undefined) ??
          WebClientMode.kUnknown,
      panelSize: null,
      resizeDuration: timeDeltaFromClient(
          result.openPanelInfo?.resizeParams?.options?.durationMs),
      canUserResize: result.openPanelInfo?.canUserResize ?? true,
    };
    if (result.openPanelInfo?.resizeParams) {
      const size = {
        width: result.openPanelInfo?.resizeParams?.width,
        height: result.openPanelInfo?.resizeParams?.height,
      };
      this.embedder.onGuestResizeRequest(size);
      openPanelInfoMojo.panelSize = size;
    }
    return {openPanelInfo: openPanelInfoMojo};
  }

  notifyPanelWasClosed(): Promise<void> {
    this.host.panelOpenStateChanged(PanelOpenState.CLOSED);
    return this.sender.requestWithResponse(
        'glicWebClientNotifyPanelWasClosed', undefined);
  }

  notifyPanelStateChange(panelState: PanelStateMojo) {
    this.sender.requestNoResponse('glicWebClientPanelStateChanged', {
      panelState: panelStateToClient(panelState),
    });
  }

  notifyPanelCanAttachChange(canAttach: boolean) {
    this.sender.requestNoResponse(
        'glicWebClientCanAttachStateChanged', {canAttach});
  }

  notifyMicrophonePermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyMicrophonePermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyLocationPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyLocationPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyTabContextPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyTabContextPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyOsLocationPermissionStateChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyOsLocationPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyClosedCaptioningSettingChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyClosedCaptioningSettingChanged', {
          enabled: enabled,
        });
  }

  notifyDefaultTabContextPermissionStateChanged(enabled: boolean) {
    this.sender.requestNoResponse(
        'glicWebClientNotifyDefaultTabContextPermissionStateChanged', {
          enabled: enabled,
        });
  }

  notifyActuationOnWebSettingChanged(enabled: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyActuationOnWebSettingChanged', {
          enabled: enabled,
        });
  }

  notifyFocusedTabChanged(focusedTabData: (FocusedTabDataMojo)): void {
    const extras = new ResponseExtras();
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifyFocusedTabChanged', {
          focusedTabDataPrivate: focusedTabDataToClient(focusedTabData, extras),
        },
        extras.transfers);
  }

  notifyPanelActiveChange(panelActive: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyPanelActiveChanged', {panelActive});
    this.host.panelIsActive = panelActive;
    this.host.updateSenderActive();
  }

  notifyManualResizeChanged(resizing: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyManualResizeChanged', {resizing});
  }

  notifyBrowserIsOpenChanged(browserIsOpen: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientBrowserIsOpenChanged', {browserIsOpen});
  }

  notifyBrowserIsActiveChanged(browserIsActive: boolean): void {
    // This isn't forwarded to the actual web client yet, as it's currently
    // only needed for the responsiveness logic, which is here.
    this.host.setBrowserIsActive(browserIsActive);
  }

  notifyOsHotkeyStateChanged(hotkey: string): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyOsHotkeyStateChanged', {hotkey});
  }

  notifyPinnedTabsChanged(tabData: TabDataMojo[]): void {
    const extras = new ResponseExtras();
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifyPinnedTabsChanged',
        {tabData: tabData.map((x) => tabDataToClient(x, extras))},
        extras.transfers);
  }

  notifyPinnedTabDataChanged(tabData: TabDataMojo): void {
    const extras = new ResponseExtras();
    this.sender.sendLatestWhenActive(
        'glicWebClientNotifyPinnedTabDataChanged',
        {tabData: tabDataToClient(tabData, extras)}, extras.transfers,
        // Cache only one entry per tab ID.
        `${tabData.tabId}`);
  }

  notifyZeroStateSuggestionsChanged(
      suggestions: ZeroStateSuggestionsV2Mojo,
      options: ZeroStateSuggestionsOptionsMojo): void {
    this.sender.sendLatestWhenActive(
        'glicWebClientZeroStateSuggestionsChanged',
        {suggestions: suggestions, options: options});
  }

  notifyActorTaskStateChanged(taskId: number, state: ActorTaskStateMojo): void {
    const clientState = state as number as ActorTaskState;
    this.sender.requestNoResponse(
        'glicWebClientNotifyActorTaskStateChanged',
        {taskId, state: clientState});
  }

  requestViewChange(requestMojo: ViewChangeRequestMojo): void {
    let request: ViewChangeRequest|undefined;
    if (requestMojo.details.actuation) {
      request = {desiredView: ClientView.ACTUATION};
    } else if (requestMojo.details.conversation) {
      request = {desiredView: ClientView.CONVERSATION};
    }
    if (!request) {
      return;
    }
    this.sender.requestNoResponse('glicWebClientRequestViewChange', {request});
  }

  notifyPageMetadataChanged(tabId: number, metadata: PageMetadataMojo|null):
      void {
    this.sender.sendLatestWhenActive(
        'glicWebClientPageMetadataChanged', {
          tabId: tabIdToClient(tabId),
          pageMetadata: pageMetadataToClient(metadata),
        },
        undefined, `${tabId}`);
  }

  async requestToShowCredentialSelectionDialog(
      request: SelectCredentialDialogRequestMojo):
      Promise<{response: SelectCredentialDialogResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'glicWebClientRequestToShowDialog',
        {request: selectCredentialDialogRequestToClient(request)});
    return {
      response: selectCredentialDialogResponseToMojo(clientResponse.response),
    };
  }

  async requestToShowUserConfirmationDialog(
      request: UserConfirmationDialogRequestMojo):
      Promise<{response: UserConfirmationDialogResponseMojo}> {
    const clientResponse = await this.sender.requestWithResponse(
        'glicWebClientRequestToShowConfirmationDialog',
        {request: userConfirmationDialogRequestToClient(request)});
    return {
      response: userConfirmationDialogResponseToMojo(clientResponse.response),
    };
  }

  notifyAdditionalContext(context: AdditionalContextMojo): void {
    const extras = new ResponseExtras();
    const clientParts = context.parts.map(p => {
      const part: AdditionalContextPartPrivate = {};
      if (p.data) {
        part.data = contextDataToClient(p.data, extras);
      } else if (p.screenshot) {
        part.screenshot = screenshotToClient(p.screenshot, extras);
      } else if (p.webPageData) {
        part.webPageData = webPageDataToClient(p.webPageData);
      } else if (p.annotatedPageData) {
        part.annotatedPageData =
            annotatedPageDataToClient(p.annotatedPageData, extras);
      } else if (p.pdfDocumentData) {
        part.pdf = pdfDocumentDataToClient(p.pdfDocumentData, extras);
      }
      return part;
    });

    const clientContext: AdditionalContextPrivate = {
      name: optionalToClient(context.name),
      tabId: tabIdToClient(context.tabId),
      origin: originToClient(context.origin),
      frameUrl: urlToClient(context.frameUrl),
      parts: clientParts,
    };

    this.sender.sendWhenActive(
        'glicWebClientNotifyAdditionalContext', {context: clientContext},
        extras.transfers);
  }
}

class PinCandidatesObserverImpl implements PinCandidatesObserver {
  receiver?: PinCandidatesObserverReceiver;
  constructor(
      private sender: GatedSender, private handler: WebClientHandlerInterface,
      private options: GetPinCandidatesOptions, public observationId: number) {
    this.connectToSource();
  }

  // Stops requesting updates. This should be called on destruction, as well as
  // when the panel is hidden to avoid incurring unnecessary costs.
  disconnectFromSource() {
    if (!this.receiver) {
      return;
    }
    this.receiver.$.close();
    this.receiver = undefined;
  }

  // Start/resume requesting updates.
  connectToSource() {
    if (this.receiver) {
      return;
    }
    this.receiver = new PinCandidatesObserverReceiver(this);
    this.handler.subscribeToPinCandidates(
        getPinCandidatesOptionsFromClient(this.options),
        this.receiver.$.bindNewPipeAndPassRemote());
  }

  onPinCandidatesChanged(candidates: PinCandidateMojo[]): void {
    const extras = new ResponseExtras();
    this.sender.sendLatestWhenActive(
        'glicWebClientPinCandidatesChanged', {
          candidates:
              candidates.map(c => ({
                               tabData: tabDataToClient(c.tabData, extras),
                             })),
          observationId: this.observationId,
        },
        extras.transfers);
  }
}

/**
 * Handles all requests to the host.
 *
 * Each function is a message handler, automatically called when the host
 * receives a message with the corresponding request name.
 *
 * Any new state or function that's not a handler should be added to
 * `GlicApiHost`.
 */
class HostMessageHandler implements HostMessageHandlerInterface {
  // Undefined until the web client is initialized.
  private receiver: WebClientReceiver|undefined;

  // Reminder: Don't add more state here! See `HostMessageHandler`'s comment.

  constructor(
      private handler: WebClientHandlerInterface, private sender: GatedSender,
      private embedder: ApiHostEmbedder, private host: GlicApiHost) {}

  destroy() {
    if (this.receiver) {
      this.receiver.$.close();
      this.receiver = undefined;
    }
  }

  async glicBrowserWebClientCreated(_request: void, extras: ResponseExtras):
      Promise<{initialState: WebClientInitialStatePrivate}> {
    if (this.receiver) {
      throw new Error('web client already created');
    }
    this.host.detailedWebClientState =
        DetailedWebClientState.WEB_CLIENT_NOT_INITIALIZED;

    const webClientImpl = new WebClientImpl(this.host, this.embedder);
    this.receiver = new WebClientReceiver(webClientImpl);
    const {initialState} = await this.handler.webClientCreated(
        this.receiver.$.bindNewPipeAndPassRemote());
    this.host.setInitialState(initialState);
    const chromeVersion = initialState.chromeVersion.components;
    const hostCapabilities = initialState.hostCapabilities;
    this.host.setBrowserIsActive(initialState.browserIsActive);

    // If the panel isn't active, don't send the focused tab until later.
    if (initialState.enableApiActivationGating && !initialState.panelIsActive) {
      const actualFocus = initialState.focusedTabData;
      initialState.focusedTabData = {
        noFocusedTabData: {
          activeTabData: null,
          noFocusReason: 'glic not active',
        },
      };
      // Note: this will queue up the message, and not send it right awway.
      webClientImpl.notifyFocusedTabChanged(actualFocus);
    }

    return {
      initialState: replaceProperties(initialState, {
        panelState: panelStateToClient(initialState.panelState),
        focusedTabData:
            focusedTabDataToClient(initialState.focusedTabData, extras),
        chromeVersion: {
          major: chromeVersion[0] || 0,
          minor: chromeVersion[1] || 0,
          build: chromeVersion[2] || 0,
          patch: chromeVersion[3] || 0,
        },
        loggingEnabled: loadTimeData.getBoolean('loggingEnabled'),
        hostCapabilities: hostCapabilitiesToClient(hostCapabilities),
      }),
    };
  }

  glicBrowserWebClientInitialized(
      request: {success: boolean, exception?: TransferableException}) {
    // The webview may have been re-shown by webui, having previously been
    // opened by the browser. In that case, show the guest frame again.

    if (request.exception) {
      console.warn(exceptionFromTransferable(request.exception));
    }

    if (request.success) {
      this.handler.webClientInitialized();
      this.host.webClientInitialized();
    } else {
      this.handler.webClientInitializeFailed();
      this.host.webClientInitializeFailed();
    }
  }

  async glicBrowserCreateTab(request: {
    url: string,
    options: {openInBackground?: boolean, windowId?: string},
  }) {
    const response = await this.handler.createTab(
        urlFromClient(request.url),
        request.options.openInBackground !== undefined ?
            request.options.openInBackground :
            false,
        optionalWindowIdFromClient(request.options.windowId));
    const tabData = response.tabData;
    if (tabData) {
      return {
        tabData: {
          tabId: tabIdToClient(tabData.tabId),
          windowId: windowIdToClient(tabData.windowId),
          url: urlToClient(tabData.url),
          title: optionalToClient(tabData.title),
        },
      };
    }
    return {};
  }

  glicBrowserOpenGlicSettingsPage(request: {options?: OpenSettingsOptions}):
      void {
    const optionsMojo: OpenSettingsOptionsMojo = {
      highlightField: SettingsPageFieldMojo.kNone,
    };
    if (request.options?.highlightField) {
      optionsMojo.highlightField = request.options?.highlightField as number;
    }
    this.handler.openGlicSettingsPage(optionsMojo);
  }

  glicBrowserClosePanel(): void {
    return this.handler.closePanel();
  }

  glicBrowserClosePanelAndShutdown(): void {
    this.handler.closePanelAndShutdown();
  }

  glicBrowserAttachPanel(): void {
    this.handler.attachPanel();
  }

  glicBrowserDetachPanel(): void {
    this.handler.detachPanel();
  }

  glicBrowserShowProfilePicker(): void {
    this.handler.showProfilePicker();
  }

  glicBrowserGetModelQualityClientId():
      Promise<{modelQualityClientId: string}> {
    return this.handler.getModelQualityClientId();
  }

  async glicBrowserSwitchConversation(request: {info?: ConversationInfo}):
      Promise<{}> {
    const {errorReason} =
        await this.handler.switchConversation(request.info ?? null);
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl(
          'switchConversation', errorReason.valueOf());
    }
    return {};
  }

  async glicBrowserRegisterConversation(request: {info: ConversationInfo}):
      Promise<{}> {
    const {errorReason} = await this.handler.registerConversation(request.info);
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl(
          'registerConversation', errorReason.valueOf());
    }
    return {};
  }

  async glicBrowserGetContextFromFocusedTab(
      request: {options: TabContextOptions}, extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextFromFocusedTab(
            tabContextOptionsFromClient(request.options));
    if (!tabContext) {
      throw new Error(`tabContext failed: ${errorReason}`);
    }
    const tabContextResult = tabContextToClient(tabContext, extras);

    return {
      tabContextResult: tabContextResult,
    };
  }

  async glicBrowserGetContextFromTab(
      request: {tabId: string, options: TabContextOptions},
      extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextFromTab(
            tabIdFromClient(request.tabId),
            tabContextOptionsFromClient(request.options));
    if (!tabContext) {
      throw new Error(`tabContext failed: ${errorReason}`);
    }
    const tabContextResult = tabContextToClient(tabContext, extras);

    return {
      tabContextResult: tabContextResult,
    };
  }

  async glicBrowserGetContextForActorFromTab(
      request: {tabId: string, options: TabContextOptions},
      extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.getContextForActorFromTab(
            tabIdFromClient(request.tabId),
            tabContextOptionsFromClient(request.options));
    if (!tabContext) {
      throw new Error(`tabContext failed: ${errorReason}`);
    }
    const tabContextResult = tabContextToClient(tabContext, extras);

    return {
      tabContextResult: tabContextResult,
    };
  }

  async glicBrowserSetMaximumNumberOfPinnedTabs(request: {
    requestedMax: number,
  }): Promise<{effectiveMax: number}> {
    const requestedMax = request.requestedMax >= 0 ? request.requestedMax : 0;
    const {effectiveMax} =
        await this.handler.setMaximumNumberOfPinnedTabs(requestedMax);
    return {effectiveMax};
  }

  async glicBrowserCreateTask(request: {taskOptions?: TaskOptions}):
      Promise<{taskId: number}> {
    try {
      const taskId =
          await this.handler.createTask(taskOptionsToMojo(request.taskOptions));
      return {
        taskId: taskId,
      };
    } catch (errorReason) {
      throw new ErrorWithReasonImpl(
          'createTask',
          (errorReason as CreateTaskErrorReason | undefined) ??
              CreateTaskErrorReason.UNKNOWN);
    }
  }

  async glicBrowserPerformActions(request: {actions: ArrayBuffer}):
      Promise<{actionsResult: ArrayBuffer}> {
    try {
      const resultProto = await this.handler.performActions(
          byteArrayFromClient(request.actions));
      const buffer = getArrayBufferFromBigBuffer(resultProto.smuggled);
      if (!buffer) {
        throw PerformActionsErrorReason.UNKNOWN;
      }
      return {
        actionsResult: buffer,
      };
    } catch (errorReason) {
      throw new ErrorWithReasonImpl(
          'performActions',
          (errorReason as PerformActionsErrorReason | undefined) ??
              PerformActionsErrorReason.UNKNOWN);
    }
  }

  glicBrowserStopActorTask(
      request: {taskId: number, stopReason: ActorTaskStopReason}): void {
    const actorTaskStopReason =
        request.stopReason as number as ActorTaskStopReasonMojo;
    this.handler.stopActorTask(request.taskId, actorTaskStopReason);
  }

  glicBrowserPauseActorTask(request: {
    taskId: number,
    pauseReason: ActorTaskPauseReason,
    tabId: string,
  }): void {
    const actorTaskPauseReason =
        request.pauseReason as number as ActorTaskPauseReasonMojo;
    this.handler.pauseActorTask(
        request.taskId, actorTaskPauseReason, tabIdFromClient(request.tabId));
  }

  async glicBrowserResumeActorTask(
      request: {taskId: number, tabContextOptions: TabContextOptions},
      extras: ResponseExtras):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {result: {errorReason, tabContext}} =
        await this.handler.resumeActorTask(
            request.taskId,
            tabContextOptionsFromClient(request.tabContextOptions));
    if (!tabContext) {
      throw new Error(`resumeActorTask failed: ${errorReason}`);
    }
    return {
      tabContextResult: tabContextToClient(tabContext, extras),
    };
  }

  async glicBrowserResizeWindow(request: {
    size: {width: number, height: number},
    options?: {durationMs?: number},
  }) {
    this.embedder.onGuestResizeRequest(request.size);
    return await this.handler.resizeWidget(
        request.size, timeDeltaFromClient(request.options?.durationMs));
  }

  glicBrowserEnableDragResize(request: {enabled: boolean}) {
    return this.embedder.enableDragResize(request.enabled);
  }

  async glicBrowserCaptureScreenshot(_request: void, extras: ResponseExtras):
      Promise<{screenshot: Screenshot}> {
    const {
      result: {screenshot, errorReason},
    } = await this.handler.captureScreenshot();
    if (!screenshot) {
      throw new ErrorWithReasonImpl(
          'captureScreenshot',
          (errorReason as CaptureScreenshotErrorReason | undefined) ??
              CaptureScreenshotErrorReason.UNKNOWN);
    }
    const screenshotArray = new Uint8Array(screenshot.data);
    extras.addTransfer(screenshotArray.buffer);
    return {
      screenshot: {
        widthPixels: screenshot.widthPixels,
        heightPixels: screenshot.heightPixels,
        data: screenshotArray.buffer,
        mimeType: screenshot.mimeType,
        originAnnotations: {},
      },
    };
  }

  glicBrowserSetWindowDraggableAreas(request: {areas: DraggableArea[]}) {
    return this.handler.setPanelDraggableAreas(request.areas);
  }

  glicBrowserSetMinimumWidgetSize(request: {
    size: {width: number, height: number},
  }) {
    return this.handler.setMinimumPanelSize(request.size);
  }

  glicBrowserSetMicrophonePermissionState(request: {enabled: boolean}) {
    return this.handler.setMicrophonePermissionState(request.enabled);
  }

  glicBrowserSetLocationPermissionState(request: {enabled: boolean}) {
    return this.handler.setLocationPermissionState(request.enabled);
  }

  glicBrowserSetTabContextPermissionState(request: {enabled: boolean}) {
    return this.handler.setTabContextPermissionState(request.enabled);
  }

  glicBrowserSetClosedCaptioningSetting(request: {enabled: boolean}) {
    return this.handler.setClosedCaptioningSetting(request.enabled);
  }

  glicBrowserSetActuationOnWebSetting(request: {enabled: boolean}) {
    return this.handler.setActuationOnWebSetting(request.enabled);
  }

  async glicBrowserGetUserProfileInfo(_request: void, extras: ResponseExtras) {
    const {profileInfo: mojoProfileInfo} =
        await this.handler.getUserProfileInfo();
    if (!mojoProfileInfo) {
      return {};
    }

    let avatarIcon: RgbaImage|undefined;
    if (mojoProfileInfo.avatarIcon) {
      avatarIcon = bitmapN32ToRGBAImage(mojoProfileInfo.avatarIcon);
      if (avatarIcon) {
        extras.addTransfer(avatarIcon.dataRGBA);
      }
    }
    return {profileInfo: replaceProperties(mojoProfileInfo, {avatarIcon})};
  }

  glicBrowserRefreshSignInCookies(): Promise<{success: boolean}> {
    return this.handler.syncCookies();
  }

  glicBrowserSetContextAccessIndicator(request: {show: boolean}): void {
    this.handler.setContextAccessIndicator(request.show);
  }

  glicBrowserSetAudioDucking(request: {enabled: boolean}): void {
    this.handler.setAudioDucking(request.enabled);
  }

  glicBrowserOnUserInputSubmitted(request: {mode: number}): void {
    this.handler.onUserInputSubmitted(request.mode);
  }

  glicBrowserOnContextUploadStarted(): void {
    this.handler.onContextUploadStarted();
  }

  glicBrowserOnContextUploadCompleted(): void {
    this.handler.onContextUploadCompleted();
  }

  glicBrowserOnReaction(request: {reactionType: number}): void {
    this.handler.onReaction(request.reactionType);
  }

  glicBrowserOnResponseStarted(): void {
    this.handler.onResponseStarted();
  }

  glicBrowserOnResponseStopped(request: {details?: OnResponseStoppedDetails}):
      void {
    const cause = request.details?.cause;

    let causeMojo = ResponseStopCauseMojo.kUnknown;
    if (cause !== undefined) {
      switch (cause) {
        case ResponseStopCause.USER:
          causeMojo = ResponseStopCauseMojo.kUser;
          break;
        case ResponseStopCause.OTHER:
          causeMojo = ResponseStopCauseMojo.kOther;
          break;
        default:
          assertNotReached();
      }
    }
    this.handler.onResponseStopped({cause: causeMojo});
  }

  glicBrowserOnSessionTerminated(): void {
    this.handler.onSessionTerminated();
  }

  glicBrowserOnTurnCompleted(request: {model: number, duration: number}): void {
    this.handler.onTurnCompleted(
        request.model, timeDeltaFromClient(request.duration));
  }

  glicBrowserOnModelChanged(request: {model: number}): void {
    this.handler.onModelChanged(request.model);
  }

  glicBrowserLogBeginAsyncEvent(request: {
    asyncEventId: number,
    taskId: number,
    event: string,
    details: string,
  }): void {
    this.handler.logBeginAsyncEvent(
        BigInt(request.asyncEventId), request.taskId, request.event,
        request.details);
  }

  glicBrowserLogEndAsyncEvent(request: {asyncEventId: number, details: string}):
      void {
    this.handler.logEndAsyncEvent(
        BigInt(request.asyncEventId), request.details);
  }

  glicBrowserLogInstantEvent(
      request: {taskId: number, event: string, details: string}): void {
    this.handler.logInstantEvent(
        request.taskId, request.event, request.details);
  }

  glicBrowserJournalClear(): void {
    this.handler.journalClear();
  }

  async glicBrowserJournalSnapshot(
      request: {clear: boolean},
      extras: ResponseExtras): Promise<{journal: Journal}> {
    const result = await this.handler.journalSnapshot(request.clear);
    const journalArray = new Uint8Array(result.journal.data);
    extras.addTransfer(journalArray.buffer);
    return {
      journal: {
        data: journalArray.buffer,
      },
    };
  }

  glicBrowserJournalStart(
      request: {maxBytes: number, captureScreenshots: boolean}): void {
    this.handler.journalStart(
        BigInt(request.maxBytes), request.captureScreenshots);
  }

  glicBrowserJournalStop(): void {
    this.handler.journalStop();
  }

  glicBrowserJournalRecordFeedback(
      request: {positive: boolean, reason: string}): void {
    this.handler.journalRecordFeedback(request.positive, request.reason);
  }

  glicBrowserOnResponseRated(request: {positive: boolean}): void {
    this.handler.onResponseRated(request.positive);
  }

  glicBrowserOnClosedCaptionsShown(): void {
    this.handler.onClosedCaptionsShown();
  }

  async glicBrowserScrollTo(request: {params: ScrollToParams}) {
    const {params} = request;

    function getMojoSelector(): ScrollToSelectorMojo {
      const {selector} = params;
      if (selector.exactText !== undefined) {
        if (selector.exactText.searchRangeStartNodeId !== undefined &&
            params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'searchRangeStartNodeId without documentId');
        }
        return {
          exactTextSelector: {
            text: selector.exactText.text,
            searchRangeStartNodeId:
                selector.exactText.searchRangeStartNodeId ?? null,
          },
        };
      }
      if (selector.textFragment !== undefined) {
        if (selector.textFragment.searchRangeStartNodeId !== undefined &&
            params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'searchRangeStartNodeId without documentId');
        }
        return {
          textFragmentSelector: {
            textStart: selector.textFragment.textStart,
            textEnd: selector.textFragment.textEnd,
            searchRangeStartNodeId:
                selector.textFragment.searchRangeStartNodeId ?? null,
          },
        };
      }
      if (selector.node !== undefined) {
        if (params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'nodeId without documentId');
        }
        return {
          nodeSelector: {
            nodeId: selector.node.nodeId,
          },
        };
      }
      throw new ErrorWithReasonImpl(
          'scrollTo', ScrollToErrorReason.NOT_SUPPORTED);
    }

    const mojoParams = {
      highlight: params.highlight === undefined ? true : params.highlight,
      selector: getMojoSelector(),
      documentId: params.documentId ?? null,
      url: params.url ? urlFromClient(params.url) : null,
    };
    const {errorReason} = (await this.handler.scrollTo(mojoParams));
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl('scrollTo', errorReason as number);
    }
    return {};
  }

  glicBrowserSetSyntheticExperimentState(request: {
    trialName: string,
    groupName: string,
  }) {
    return this.handler.setSyntheticExperimentState(
        request.trialName, request.groupName);
  }

  glicBrowserOpenOsPermissionSettingsMenu(request: {permission: string}) {
    // Warning: calling openOsPermissionSettingsMenu with unsupported content
    // setting type will terminate the render process (bad mojo message). Update
    // GlicWebClientHandler:OpenOsPermissionSettingsMenu with any new types.
    switch (request.permission) {
      case 'media':
        return this.handler.openOsPermissionSettingsMenu(
            ContentSettingsType.MEDIASTREAM_MIC);
      case 'geolocation':
        return this.handler.openOsPermissionSettingsMenu(
            ContentSettingsType.GEOLOCATION);
    }
    return Promise.resolve();
  }

  glicBrowserGetOsMicrophonePermissionStatus(): Promise<{enabled: boolean}> {
    return this.handler.getOsMicrophonePermissionStatus();
  }

  glicBrowserPinTabs(request: {tabIds: string[]}):
      Promise<{pinnedAll: boolean}> {
    return this.handler.pinTabs(request.tabIds.map((x) => tabIdFromClient(x)));
  }

  glicBrowserUnpinTabs(request: {tabIds: string[]}):
      Promise<{unpinnedAll: boolean}> {
    return this.handler.unpinTabs(
        request.tabIds.map((x) => tabIdFromClient(x)));
  }

  glicBrowserUnpinAllTabs(): void {
    this.handler.unpinAllTabs();
  }

  glicBrowserSubscribeToPinCandidates(request: {
    options: GetPinCandidatesOptions,
    observationId: number,
  }): void {
    this.host.pinCandidatesObserver?.disconnectFromSource();
    this.host.pinCandidatesObserver = new PinCandidatesObserverImpl(
        this.sender, this.handler, request.options, request.observationId);
  }

  glicBrowserUnsubscribeFromPinCandidates(request: {observationId: number}):
      void {
    if (!this.host.pinCandidatesObserver) {
      return;
    }
    if (this.host.pinCandidatesObserver.observationId ===
        request.observationId) {
      this.host.pinCandidatesObserver.disconnectFromSource();
      this.host.pinCandidatesObserver = undefined;
    }
  }

  async glicBrowserGetZeroStateSuggestionsForFocusedTab(request: {
    isFirstRun?: boolean,
  }): Promise<{suggestions?: ZeroStateSuggestions}> {
    const zeroStateResult =
        await this.handler.getZeroStateSuggestionsForFocusedTab(
            optionalFromClient(request.isFirstRun));
    const zeroStateData = zeroStateResult.suggestions;
    if (!zeroStateData) {
      return {};
    } else {
      return {
        suggestions: {
          tabId: tabIdToClient(zeroStateData.tabId),
          url: urlToClient(zeroStateData.tabUrl),
          suggestions: zeroStateData.suggestions,
        },
      };
    }
  }

  async glicBrowserGetZeroStateSuggestionsAndSubscribe(request: {
    hasActiveSubscription: boolean,
    options: ZeroStateSuggestionsOptions,
  }): Promise<{suggestions?: ZeroStateSuggestionsV2}> {
    const zeroStateResult =
        await this.handler.getZeroStateSuggestionsAndSubscribe(
            request.hasActiveSubscription, {
              isFirstRun: request.options.isFirstRun ?? false,
              supportedTools: request.options.supportedTools ?? [],
            });
    const zeroStateData = zeroStateResult.zeroStateSuggestions;
    if (!zeroStateData) {
      return {};
    } else {
      return {suggestions: zeroStateData};
    }
  }
  glicBrowserDropScrollToHighlight(): void {
    this.handler.dropScrollToHighlight();
  }

  glicBrowserMaybeRefreshUserStatus(): void {
    this.handler.maybeRefreshUserStatus();
  }

  glicBrowserOnViewChanged(request: {notification: ViewChangedNotification}):
      void {
    const {currentView} = request.notification;
    switch (currentView) {
      case ClientView.ACTUATION:
        this.handler.onViewChanged({currentView: CurrentViewMojo.kActuation});
        break;
      case ClientView.CONVERSATION:
        this.handler.onViewChanged(
            {currentView: CurrentViewMojo.kConversation});
        break;
      default:
        // The compiler should enforce that this is unreachable if types are
        // correct; nonetheless check at runtime since TypeScript cannot
        // guarantee this absolutely.
        const _exhaustive: never = currentView;
        throw new Error(
            `glicBrowserOnViewChanged: invalid currentView: ${_exhaustive}`);
    }
  }

  glicBrowserSubscribeToPageMetadata(request: {
    tabId: string,
    names: string[],
  }): Promise<{success: boolean}> {
    return this.handler.subscribeToPageMetadata(
        tabIdFromClient(request.tabId), request.names);
  }
}

/**
 * The host side of the Glic API.
 *
 * Its primary job is to route calls between the client (over postMessage) and
 * the browser (over Mojo).
 */
export class GlicApiHost implements PostMessageRequestHandler {
  private senderId = newSenderId();
  private messageHandler: HostMessageHandler;
  private readonly postMessageReceiver: PostMessageRequestReceiver;
  sender: GatedSender;
  private enableApiActivationGating = true;
  panelIsActive = false;
  private handler: WebClientHandlerRemote;
  private bootstrapPingIntervalId: number|undefined;
  private webClientErrorTimer: OneShotTimer;
  private webClientState =
      ObservableValue.withValue<WebClientState>(WebClientState.UNINITIALIZED);
  private waitingOnPanelWillOpenValue = false;
  private clientActiveObs = ObservableValue.withValue(false);
  private panelOpenState = PanelOpenState.CLOSED;
  private browserIsActive = true;
  private hasShownDebuggerAttachedWarning = false;
  detailedWebClientState = DetailedWebClientState.BOOTSTRAP_PENDING;
  // Present while the client is monitoring pin candidates.
  pinCandidatesObserver?: PinCandidatesObserverImpl;

  constructor(
      private browserProxy: BrowserProxy, private windowProxy: WindowProxy,
      private embeddedOrigin: string, embedder: ApiHostEmbedder) {
    this.postMessageReceiver = new PostMessageRequestReceiver(
        embeddedOrigin, this.senderId, windowProxy, this, 'glic_api_host');
    this.postMessageReceiver.setLoggingEnabled(
        loadTimeData.getBoolean('loggingEnabled'));
    const ungatedSender = new PostMessageRequestSender(
        windowProxy, embeddedOrigin, this.senderId, 'glic_api_host');
    ungatedSender.setLoggingEnabled(loadTimeData.getBoolean('loggingEnabled'));
    this.sender = new GatedSender(ungatedSender);
    this.handler = new WebClientHandlerRemote();
    this.handler.onConnectionError.addListener(() => {
      if (this.webClientState.getCurrentValue() !== WebClientState.ERROR) {
        console.warn(`Mojo connection error in glic host`);
        this.detailedWebClientState =
            DetailedWebClientState.MOJO_PIPE_CLOSED_UNEXPECTEDLY;
        this.webClientState.assignAndSignal(WebClientState.ERROR);
      }
    });
    this.handler.$.close();
    this.browserProxy.handler.createWebClient(
        this.handler.$.bindNewPipeAndPassReceiver());
    this.messageHandler =
        new HostMessageHandler(this.handler, this.sender, embedder, this);
    this.webClientErrorTimer = new OneShotTimer(
        loadTimeData.getInteger('clientUnresponsiveUiMaxTimeMs'));

    this.bootstrapPingIntervalId =
        window.setInterval(this.bootstrapPing.bind(this), 50);
    this.bootstrapPing();
  }

  destroy() {
    this.webClientState = ObservableValue.withValue<WebClientState>(
        WebClientState.ERROR);  // Final state
    window.clearInterval(this.bootstrapPingIntervalId);
    this.webClientErrorTimer.reset();
    this.postMessageReceiver.destroy();
    this.messageHandler.destroy();
    this.sender.destroy();
    this.pinCandidatesObserver?.disconnectFromSource();
  }

  setInitialState(initialState: WebClientInitialState) {
    this.enableApiActivationGating = initialState.enableApiActivationGating;
    this.panelIsActive = initialState.panelIsActive;
    this.updateSenderActive();
  }

  updateSenderActive() {
    this.sender.setGating(this.shouldGateRequests());
  }

  shouldGateRequests(): boolean {
    return !this.panelIsActive && this.enableApiActivationGating;
  }

  // Called when the webview page is loaded.
  contentLoaded() {
    // Send the ping message one more time. At this point, the webview should
    // be able to handle the message, if it hasn't already.
    this.bootstrapPing();
    this.stopBootstrapPing();
  }

  waitingOnPanelWillOpen() {
    return this.waitingOnPanelWillOpenValue;
  }

  setWaitingOnPanelWillOpen(value: boolean): void {
    this.waitingOnPanelWillOpenValue = value;
  }

  panelOpenStateChanged(state: PanelOpenState) {
    this.panelOpenState = state;
    this.clientActiveObs.assignAndSignal(this.isClientActive());
    if (state === PanelOpenState.CLOSED) {
      this.pinCandidatesObserver?.disconnectFromSource();
    } else {
      this.pinCandidatesObserver?.connectToSource();
    }
  }

  setBrowserIsActive(browserIsActive: boolean) {
    this.browserIsActive = browserIsActive;
    this.clientActiveObs.assignAndSignal(this.isClientActive());
  }

  // Returns true if the user might be interacting with the client.
  // That is, the panel is open, not in an error state, and either the panel
  // itself is focused or a browser window it could be accessing is.
  private isClientActive() {
    return this.panelOpenState === PanelOpenState.OPEN &&
        this.webClientState.getCurrentValue() !== WebClientState.ERROR &&
        this.browserIsActive;
  }

  // Called when the web client is initialized.
  webClientInitialized() {
    this.detailedWebClientState = DetailedWebClientState.RESPONSIVE;
    this.setWebClientState(WebClientState.RESPONSIVE);
    this.responsiveCheckLoop();
  }

  webClientInitializeFailed() {
    console.warn('GlicApiHost: web client initialize failed');
    this.detailedWebClientState =
        DetailedWebClientState.WEB_CLIENT_INITIALIZE_FAILED;
    this.setWebClientState(WebClientState.ERROR);
  }

  setWebClientState(state: WebClientState) {
    this.webClientState.assignAndSignal(state);
  }

  getWebClientState(): ObservableValueReadOnly<WebClientState> {
    return this.webClientState;
  }

  getDetailedWebClientState(): DetailedWebClientState {
    return this.detailedWebClientState;
  }

  // Sends a message to the webview which is required to initialize the client.
  // Because we don't know when the client will be ready to receive this
  // message, we start sending this every 50ms as soon as navigation commits on
  // the webview, and stop sending this when the page loads, or we receive a
  // request from the client.
  bootstrapPing() {
    if (this.bootstrapPingIntervalId === undefined) {
      return;
    }
    this.windowProxy.postMessage(
        {
          type: 'glic-bootstrap',
          glicApiSource: loadTimeData.getString('glicGuestAPISource'),
        },
        this.embeddedOrigin);
  }

  stopBootstrapPing() {
    if (this.bootstrapPingIntervalId !== undefined) {
      window.clearInterval(this.bootstrapPingIntervalId);
      this.bootstrapPingIntervalId = undefined;
    }
  }

  async responsiveCheckLoop() {
    if (!loadTimeData.getBoolean('isClientResponsivenessCheckEnabled')) {
      return;
    }

    // Timeout duration for waiting for a response. Increased in dev mode.
    const timeoutMs: number =
        loadTimeData.getInteger('clientResponsivenessCheckTimeoutMs') *
        (loadTimeData.getBoolean('devMode') ? 1000 : 1);
    // Interval in between the consecutive checks.
    const checkIntervalMs: number =
        loadTimeData.getInteger('clientResponsivenessCheckIntervalMs');

    while (this.webClientState.getCurrentValue() !== WebClientState.ERROR) {
      if (!this.isClientActive()) {
        if (this.webClientState.getCurrentValue() ===
            WebClientState.UNRESPONSIVE) {
          this.detailedWebClientState =
              DetailedWebClientState.UNRESPONSIVE_INACTIVE;
          // Prevent unresponsive overlay showing forever while checking is
          // paused.
          this.setWebClientState(WebClientState.RESPONSIVE);
          this.webClientErrorTimer.reset();
        } else {
          this.detailedWebClientState =
              DetailedWebClientState.RESPONSIVE_INACTIVE;
        }
        await this.clientActiveObs.waitUntil((active) => active);
      }

      let gotResponse = false;
      const responsePromise =
          this.sender
              .requestWithResponse('glicWebClientCheckResponsive', undefined)
              .then(() => {
                gotResponse = true;
              });
      const responseTimeout = sleep(timeoutMs);

      await Promise.race([responsePromise, responseTimeout]);
      if (this.webClientState.getCurrentValue() === WebClientState.ERROR) {
        return;  // ERROR state is final.
      }

      if (gotResponse) {  // Success
        this.webClientErrorTimer.reset();
        this.setWebClientState(WebClientState.RESPONSIVE);
        this.detailedWebClientState = DetailedWebClientState.RESPONSIVE;

        await sleep(checkIntervalMs);
        continue;
      }

      // Failed, not responsive.
      if (this.webClientState.getCurrentValue() === WebClientState.RESPONSIVE) {
        const ignoreUnresponsiveClient =
            await this.shouldAllowUnresponsiveClient();
        if (!ignoreUnresponsiveClient) {
          console.warn('GlicApiHost: web client is unresponsive');
          this.detailedWebClientState =
              DetailedWebClientState.TEMPORARY_UNRESPONSIVE;
          this.setWebClientState(WebClientState.UNRESPONSIVE);
          this.startWebClientErrorTimer();
        }
      }

      // Crucial: Wait for the original (late) response promise to settle before
      // the next check cycle starts.
      await responsePromise;
    }
  }

  private async shouldAllowUnresponsiveClient(): Promise<boolean> {
    if (loadTimeData.getBoolean(
            'clientResponsivenessCheckIgnoreWhenDebuggerAttached')) {
      const isDebuggerAttached: boolean =
          await this.handler.isDebuggerAttached()
              .then(result => result.isAttachedToWebview)
              .catch(() => false);

      if (isDebuggerAttached) {
        if (!this.hasShownDebuggerAttachedWarning) {
          console.warn(
              'GlicApiHost: ignoring unresponsive client because ' +
              'a debugger (likely DevTools) is attached');
          this.hasShownDebuggerAttachedWarning = true;
        }
        return true;
      }
    }

    return false;
  }

  startWebClientErrorTimer() {
    this.webClientErrorTimer.start(() => {
      console.warn('GlicApiHost: web client is permanently unresponsive');
      this.detailedWebClientState =
          DetailedWebClientState.PERMANENT_UNRESPONSIVE;
      this.setWebClientState(WebClientState.ERROR);
    });
  }

  async openLinkInNewTab(url: string) {
    await this.handler.createTab(urlFromClient(url), false, null);
  }

  async shouldAllowMediaPermissionRequest(): Promise<boolean> {
    return (await this.handler.shouldAllowMediaPermissionRequest()).isAllowed;
  }

  async shouldAllowGeolocationPermissionRequest(): Promise<boolean> {
    return (await this.handler.shouldAllowGeolocationPermissionRequest())
        .isAllowed;
  }

  // PostMessageRequestHandler implementation.
  async handleRawRequest(type: string, payload: any, extras: ResponseExtras):
      Promise<{payload: any}|undefined> {
    const handlerFunction = (this.messageHandler as any)[type];
    if (typeof handlerFunction !== 'function') {
      console.warn(`GlicApiHost: Unknown message type ${type}`);
      return;
    }

    if (this.detailedWebClientState ===
        DetailedWebClientState.BOOTSTRAP_PENDING) {
      this.detailedWebClientState =
          DetailedWebClientState.WEB_CLIENT_NOT_CREATED;
    }
    this.stopBootstrapPing();

    let response;
    if (this.shouldGateRequests() &&
        Object.hasOwn(BACKGROUND_RESPONSES, type)) {
      const backgroundResponse =
          BACKGROUND_RESPONSES[type as keyof typeof BACKGROUND_RESPONSES] as
          HostBackgroundResponse<any>;
      if (Object.hasOwn(backgroundResponse, 'throws')) {
        const friendlyName =
            type.replaceAll(/^glicBrowser|^glicWebClient/g, '');
        throw new Error(`${friendlyName} not allowed while backgrounded`);
      }
      if (Object.hasOwn(backgroundResponse, 'does')) {
        response = await (backgroundResponse as HostBackgroundResponseDoes<any>)
                       .does();
      } else {
        response =
            (backgroundResponse as HostBackgroundResponseReturns<any>).returns;
      }
    } else {
      response =
          await handlerFunction.call(this.messageHandler, payload, extras);
    }
    if (!response) {
      // Not all request types require a return value.
      return;
    }
    return {payload: response};
  }

  onRequestReceived(type: string): void {
    this.reportRequestCountEvent(type, GlicRequestEvent.REQUEST_RECEIVED);
    if (document.visibilityState === 'hidden') {
      this.reportRequestCountEvent(
          type, GlicRequestEvent.REQUEST_RECEIVED_WHILE_HIDDEN);
    }
  }

  onRequestHandlerException(type: string): void {
    this.reportRequestCountEvent(
        type, GlicRequestEvent.REQUEST_HANDLER_EXCEPTION);
  }

  onRequestCompleted(type: string): void {
    this.reportRequestCountEvent(type, GlicRequestEvent.RESPONSE_SENT);
  }

  reportRequestCountEvent(requestType: string, event: GlicRequestEvent) {
    const histogramSuffix = requestTypeToHistogramSuffix(requestType);
    if (histogramSuffix === undefined) {
      return;
    }
    const requestTypeNumber: number|undefined =
        (HOST_REQUEST_TYPES as any)[histogramSuffix];
    if (!requestTypeNumber) {
      console.warn(
          `reportRequestCountEvent: invalid requestType ${histogramSuffix}`);
      return;
    }
    chrome.metricsPrivate.recordEnumerationValue(
        `Glic.Api.RequestCounts.${histogramSuffix}`, event,
        GlicRequestEvent.MAX_VALUE + 1);

    switch (event) {
      case GlicRequestEvent.REQUEST_HANDLER_EXCEPTION:
        chrome.metricsPrivate.recordEnumerationValue(
            `Glic.Api.RequestCounts.Error`, requestTypeNumber,
            HOST_REQUEST_TYPES.MAX_VALUE + 1);
        break;
      case GlicRequestEvent.REQUEST_RECEIVED_WHILE_HIDDEN:
        chrome.metricsPrivate.recordEnumerationValue(
            `Glic.Api.RequestCounts.Hidden`, requestTypeNumber,
            HOST_REQUEST_TYPES.MAX_VALUE + 1);
        break;
      case GlicRequestEvent.REQUEST_RECEIVED:
        chrome.metricsPrivate.recordEnumerationValue(
            `Glic.Api.RequestCounts.Received`, requestTypeNumber,
            HOST_REQUEST_TYPES.MAX_VALUE + 1);
        break;
      default:
        break;
    }
  }
}

interface QueuedMessage {
  order: number;
  requestType: string;
  payload: any;
  transfer: Transferable[];
}

// Sends messages to the client, subject to the `backgroundAllowed` property.
// Supports queueing of messages not `backgroundAllowed`.
export class GatedSender {
  private sequenceNumber = 0;
  private messageQueue: QueuedMessage[] = [];
  private keyedMessages = new Map<string, QueuedMessage>();
  private shouldGateRequests = true;
  constructor(private sender: PostMessageRequestSender) {}

  // This is an escape hatch which should be used sparingly.
  getRawSender(): PostMessageRequestSender {
    return this.sender;
  }

  destroy() {
    this.sender.destroy();
  }

  setGating(shouldGateRequests: boolean): void {
    if (this.shouldGateRequests === shouldGateRequests) {
      return;
    }
    this.shouldGateRequests = shouldGateRequests;
    if (this.shouldGateRequests) {
      return;
    }

    // Sort and send the queued messages.
    const messages = this.messageQueue;
    this.messageQueue = [];
    messages.push(...this.keyedMessages.values());
    this.keyedMessages.clear();
    messages.sort((a, b) => a.order - b.order);
    messages.forEach((message) => {
      this.sender.requestNoResponse(
          message.requestType as any, message.payload, message.transfer);
    });
  }

  // Sends a request whenever glic is active.
  // Queues the request for later if glic is backgrounded.
  sendWhenActive<T extends keyof AllRequestTypesWithoutReturn>(
      requestType: T, request: RequestRequestType<T>,
      transfer: Transferable[] = []): void {
    if (!this.shouldGateRequests) {
      this.sender.requestNoResponse(requestType, request, transfer);
    } else {
      this.messageQueue.push({
        order: this.sequenceNumber++,
        requestType,
        payload: request,
        transfer,
      });
    }
  }

  // Sends a request only if glic is active, otherwise it is dropped.
  sendIfActiveOrDrop<T extends keyof AllRequestTypesWithoutReturn>(
      requestType: T, request: RequestRequestType<T>,
      transfer: Transferable[] = []): void {
    if (!this.shouldGateRequests) {
      this.sender.requestNoResponse(requestType, request, transfer);
    }
  }

  // Sends a request if glic is active, otherwise the request is queued for
  // later. If more than one request has the same key
  // `${requestType},${additionalKey}`, only the last request is saved in the
  // queue.
  sendLatestWhenActive<T extends keyof AllRequestTypesWithoutReturn>(
      requestType: T, request: RequestRequestType<T>,
      transfer: Transferable[] = [], additionalKey?: string): void {
    if (!this.shouldGateRequests) {
      this.sender.requestNoResponse(requestType, request, transfer);
    } else {
      let key: string = requestType;
      if (additionalKey) {
        key += ',' + additionalKey;
      }
      this.keyedMessages.set(key, {
        order: this.sequenceNumber++,
        requestType,
        payload: request,
        transfer,
      });
    }
  }

  // Sends a request without waiting for a response. Allowed only for
  // backgroundAllowed request types.
  requestNoResponse < T extends keyof
  Omit < UngatedWebClientRequestTypes,
      keyof AllRequestTypesWithReturn >> (requestType: T,
                                          request: RequestRequestType<T>,
                                          transfer: Transferable[] = []): void {
    this.sender.requestNoResponse(requestType, request, transfer);
  }

  // Sends a request and waits for a response. Allowed only for
  // backgroundAllowed request types.
  requestWithResponse<T extends keyof UngatedWebClientRequestTypes>(
      requestType: T, request: RequestRequestType<T>,
      transfer: Transferable[] = []) {
    return this.sender.requestWithResponse(requestType, request, transfer);
  }
}

// LINT.IfChange(GlicRequestEvent)
enum GlicRequestEvent {
  REQUEST_RECEIVED = 0,
  RESPONSE_SENT = 1,
  REQUEST_HANDLER_EXCEPTION = 2,
  REQUEST_RECEIVED_WHILE_HIDDEN = 3,
  MAX_VALUE = REQUEST_RECEIVED_WHILE_HIDDEN,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicRequestEvent)

// Returns a Promise resolving after 'ms' milliseconds
function sleep(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// Utility functions for converting from mojom types to message types.
// Summary of changes:
// * Window and tab IDs are sent using int32 in mojo, but made opaque
//   strings for the public API. This allows Chrome to change the ID
//   representation later.
// * Optional types in Mojo use null, but optional types in the public API use
//   undefined.
function windowIdToClient(windowId: number): string {
  return `${windowId}`;
}

function windowIdFromClient(windowId: string): number {
  return parseInt(windowId);
}

function tabIdToClient(tabId: number): string;
function tabIdToClient(tabId: number|null): string|undefined;
function tabIdToClient(tabId: number|null): string|undefined {
  if (tabId === null) {
    return undefined;
  }
  return `${tabId}`;
}

function tabIdFromClient(tabId: string): number {
  const parsed = parseInt(tabId);
  if (Number.isNaN(parsed)) {
    return 0;
  }
  return parsed;
}

function optionalWindowIdToClient(windowId: number|null): string|undefined {
  if (windowId === null) {
    return undefined;
  }
  return windowIdToClient(windowId);
}

function optionalWindowIdFromClient(windowId: string|undefined): number|null {
  if (windowId === undefined) {
    return null;
  }
  return windowIdFromClient(windowId);
}

function screenshotToClient(
    screenshot: ScreenshotMojo|null, extras: ResponseExtras): Screenshot|
    undefined {
  if (!screenshot) {
    return undefined;
  }
  const screenshotArray = new Uint8Array(screenshot.data);
  const buffer = screenshotArray.buffer;
  extras.addTransfer(buffer);
  return {
    widthPixels: screenshot.widthPixels,
    heightPixels: screenshot.heightPixels,
    data: buffer,
    mimeType: screenshot.mimeType,
    originAnnotations: {},
  };
}

function contextDataToClient(data: ContextDataMojo, extras: ResponseExtras):
    {mimeType: string, data: ArrayBuffer}|undefined {
  const buffer = getArrayBufferFromBigBuffer(data.data);
  if (!buffer) {
    return undefined;
  }
  extras.addTransfer(buffer);
  return {mimeType: data.mimeType, data: buffer};
}

function webPageDataToClient(webPageData: WebPageDataMojo|null): WebPageData|
    undefined {
  if (!webPageData) {
    return undefined;
  }
  return {
    mainDocument: {
      origin: originToClient(webPageData.mainDocument.origin),
      innerText: webPageData.mainDocument.innerText,
      innerTextTruncated: webPageData.mainDocument.innerTextTruncated,
    },
  };
}

function pdfDocumentDataToClient(
    pdfDocumentData: PdfDocumentDataMojo|null,
    extras: ResponseExtras): PdfDocumentDataPrivate|undefined {
  if (!pdfDocumentData) {
    return undefined;
  }
  const pdfData = pdfDocumentData.pdfData ?
      new Uint8Array(pdfDocumentData.pdfData).buffer :
      undefined;
  if (pdfData) {
    extras.addTransfer(pdfData);
  }
  return {
    origin: originToClient(pdfDocumentData.origin),
    pdfSizeLimitExceeded: pdfDocumentData.sizeLimitExceeded,
    pdfData,
  };
}

function annotatedPageDataToClient(
    annotatedPageData: AnnotatedPageDataMojo|null,
    extras: ResponseExtras): AnnotatedPageDataPrivate|undefined {
  if (!annotatedPageData) {
    return undefined;
  }
  const annotatedPageContent = annotatedPageData.annotatedPageContent ?
      getArrayBufferFromBigBuffer(
          annotatedPageData.annotatedPageContent.smuggled) :
      undefined;
  if (annotatedPageContent) {
    extras.addTransfer(annotatedPageContent);
  }
  let metadata: PageMetadata|undefined = undefined;
  if (annotatedPageData.metadata) {
    metadata = {
      frameMetadata: annotatedPageData.metadata.frameMetadata.map(
          m => replaceProperties(m, {url: urlToClient(m.url)})),
    };
  }
  return {annotatedPageContent, metadata};
}

function optionalToClient<T>(value: T|null) {
  if (value === null) {
    return undefined;
  }
  return value;
}

function optionalFromClient<T>(value: T|undefined) {
  if (value === undefined) {
    return null;
  }
  return value;
}

function urlToClient(url: Url): string;
function urlToClient(url: Url|null): string|undefined;
function urlToClient(url: Url|null): string|undefined {
  if (url === null) {
    return undefined;
  }
  return url.url;
}

function urlFromClient(url: string): Url {
  return {url};
}

function originToClient(origin: Origin): string;
function originToClient(origin: Origin|null): string|undefined;
function originToClient(origin: Origin|null): string|undefined {
  if (!origin) {
    return undefined;
  }
  if (!origin.scheme) {
    return '';
  }
  const originBase = `${origin.scheme}://${origin.host}`;
  if (origin.port) {
    return `${originBase}:${origin.port}`;
  }
  return originBase;
}

function tabDataToClient(
    tabData: TabDataMojo, extras: ResponseExtras): TabDataPrivate;
function tabDataToClient(tabData: TabDataMojo|null, extras: ResponseExtras):
    TabDataPrivate|undefined;
function tabDataToClient(tabData: TabDataMojo|null, extras: ResponseExtras):
    TabDataPrivate|undefined {
  if (!tabData) {
    return undefined;
  }

  let favicon: RgbaImage|undefined = undefined;
  if (tabData.favicon) {
    favicon = bitmapN32ToRGBAImage(tabData.favicon);
    if (favicon) {
      extras.addTransfer(favicon.dataRGBA);
    }
  }

  const isObservable = optionalToClient(tabData.isObservable);
  const isMediaActive = optionalToClient(tabData.isMediaActive);
  const isTabContentCaptured = optionalToClient(tabData.isTabContentCaptured);
  return {
    tabId: tabIdToClient(tabData.tabId),
    windowId: windowIdToClient(tabData.windowId),
    url: urlToClient(tabData.url),
    title: optionalToClient(tabData.title),
    favicon,
    faviconUrl: urlToClient(tabData.faviconUrl),
    documentMimeType: tabData.documentMimeType,
    isObservable,
    isMediaActive,
    isTabContentCaptured,
  };
}

function focusedTabDataToClient(
    focusedTabData: FocusedTabDataMojo,
    extras: ResponseExtras): FocusedTabDataPrivate {
  if (focusedTabData.focusedTab) {
    return {
      hasFocus: {tabData: tabDataToClient(focusedTabData.focusedTab, extras)},
    };
  }
  if (focusedTabData.noFocusedTabData) {
    return {
      hasNoFocus: {
        tabFocusCandidateData: tabDataToClient(
            focusedTabData.noFocusedTabData.activeTabData, extras),
        noFocusReason: focusedTabData.noFocusedTabData.noFocusReason,
      },
    };
  }
  console.error('Invalid FocusedTabDataMojo');
  return {};
}

function getArrayBufferFromBigBuffer(bigBuffer: BigBuffer): ArrayBuffer|
    undefined {
  if (bigBuffer.bytes !== undefined) {
    return new Uint8Array(bigBuffer.bytes).buffer;
  }
  return bigBuffer.sharedMemory?.bufferHandle
      .mapBuffer(0, bigBuffer.sharedMemory.size)
      .buffer;
}

function bitmapN32ToRGBAImage(bitmap: BitmapN32): RgbaImage|undefined {
  const bytes = getArrayBufferFromBigBuffer(bitmap.pixelData);
  if (!bytes) {
    return undefined;
  }
  // We don't transmit ColorType over mojo, because it's determined by the
  // endianness of the platform. Chromium only supports little endian, which
  // maps to BGRA. See third_party/skia/include/core/SkColorType.h.
  const colorType = ImageColorType.BGRA;

  return {
    width: bitmap.imageInfo.width,
    height: bitmap.imageInfo.height,
    dataRGBA: bytes,
    alphaType: bitmap.imageInfo.alphaType === AlphaType.PREMUL ?
        ImageAlphaType.PREMUL :
        ImageAlphaType.UNPREMUL,
    colorType,
  };
}

function panelOpeningDataToClient(panelOpeningData: PanelOpeningDataMojo):
    PanelOpeningData {
  return {
    panelState: panelStateToClient(panelOpeningData.panelState),
    invocationSource: panelOpeningData.invocationSource as number,
    conversationId: optionalToClient(panelOpeningData.conversationId),
  };
}

function panelStateToClient(panelState: PanelStateMojo): PanelState {
  return {
    kind: panelState.kind as number,
    windowId: optionalWindowIdToClient(panelState.windowId),
  };
}

function pageMetadataToClient(metadata: PageMetadataMojo|null): PageMetadata|
    null {
  if (!metadata) {
    return null;
  }
  return {
    frameMetadata: metadata.frameMetadata.map(
        m => replaceProperties(m, {url: urlToClient(m.url)})),
  };
}

/** Takes a time value in milliseconds and converts to a Mojo TimeDelta. */
function timeDeltaFromClient(durationMs: number = 0): TimeDelta {
  if (!Number.isFinite(durationMs)) {
    throw new Error('Invalid duration value: ' + durationMs);
  }
  return {microseconds: BigInt(Math.floor(durationMs * 1000))};
}

function tabContextToClient(tabContext: TabContextMojo, extras: ResponseExtras):
    TabContextResultPrivate {
  const tabData: TabDataPrivate = tabDataToClient(tabContext.tabData, extras);
  const webPageData = webPageDataToClient(tabContext.webPageData);
  const viewportScreenshot =
      screenshotToClient(tabContext.viewportScreenshot, extras);
  const pdfDocumentData =
      pdfDocumentDataToClient(tabContext.pdfDocumentData, extras);
  const annotatedPageData =
      annotatedPageDataToClient(tabContext.annotatedPageData, extras);

  return {
    tabData,
    webPageData,
    viewportScreenshot,
    pdfDocumentData,
    annotatedPageData,
  };
}

function tabContextOptionsFromClient(options: TabContextOptions):
    TabContextOptionsMojo {
  return {
    includeInnerText: options.innerText ?? false,
    innerTextBytesLimit:
        options.innerTextBytesLimit ?? DEFAULT_INNER_TEXT_BYTES_LIMIT,
    includeViewportScreenshot: options.viewportScreenshot ?? false,
    includePdf: options.pdfData ?? false,
    includeAnnotatedPageContent: options.annotatedPageContent ?? false,
    maxMetaTags: options.maxMetaTags ?? 0,
    pdfSizeLimit: options.pdfSizeLimit === undefined ?
        DEFAULT_PDF_SIZE_LIMIT :
        Math.min(Number.MAX_SAFE_INTEGER, options.pdfSizeLimit),
    annotatedPageContentMode: options.annotatedPageContentMode === undefined ?
        0 :
        options.annotatedPageContentMode,
  };
}

// Taken from mojo_type_utils.ts
function getPinCandidatesOptionsFromClient(options: GetPinCandidatesOptions):
    GetPinCandidatesOptionsMojo {
  return {
    maxCandidates: options.maxCandidates,
    query: options.query ?? null,
  };
}

function byteArrayFromClient(buffer: ArrayBuffer): number[] {
  const byteArray = new Uint8Array(buffer);
  return Array.from(byteArray);
}

function hostCapabilitiesToClient(capabilities: HostCapabilityMojo[]):
    HostCapability[] {
  return capabilities.map(capability => capability as number as HostCapability);
}

function selectCredentialDialogResponseToMojo(
    response: SelectCredentialDialogResponsePrivate):
    SelectCredentialDialogResponseMojo {
  return response.errorReason ?
      {
        taskId: response.taskId,
        errorReason: response.errorReason as number as
            SelectCredentialDialogErrorReasonMojo,
        permissionDuration: null,
        selectedCredentialId: null,
      } :
      {
        ...response,
        errorReason: null,
        permissionDuration: optionalFromClient(response.permissionDuration) as
                UserGrantedPermissionDurationMojo |
            null,
        selectedCredentialId: response.selectedCredentialId ?? null,
      };
}

function selectCredentialDialogRequestToClient(
    request: SelectCredentialDialogRequestMojo):
    SelectCredentialDialogRequestPrivate {
  const icons = new Map<string, RgbaImage>();
  if (request.icons) {
    for (const [siteOrApp, value] of Object.entries(request.icons)) {
      const rgbaImage = bitmapN32ToRGBAImage(value);
      if (rgbaImage) {
        icons.set(siteOrApp, rgbaImage);
      }
    }
  }
  return {
    ...request,
    icons,
  };
}

function userConfirmationDialogRequestToClient(
    request: UserConfirmationDialogRequestMojo):
    UserConfirmationDialogRequestPrivate {
  return {
    navigationOrigin: request.payload.navigationOrigin ?
        originToClient(request.payload.navigationOrigin) :
        undefined,
    downloadId: typeof request.payload.downloadId === 'number' ?
        request.payload.downloadId :
        undefined,
  };
}

function userConfirmationDialogResponseToMojo(
    response: UserConfirmationDialogResponsePrivate):
    UserConfirmationDialogResponseMojo {
  if (response.errorReason) {
    return {
      result: {
        errorReason: response.errorReason as number as
            UserConfirmationDialogErrorReasonMojo,
      },
    };
  }
  return {
    result: {permissionGranted: response.permissionGranted},
  };
}

function taskOptionsToMojo(taskOptions?: TaskOptions): TaskOptionsMojo|null {
  if (taskOptions) {
    return {
      title: taskOptions.title ?? null,
    };
  }
  return null;
}
