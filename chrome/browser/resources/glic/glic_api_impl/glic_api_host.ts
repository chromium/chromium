// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in
// glic_api_host/glic_api_impl.ts.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {BitmapN32} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import {AlphaType} from '//resources/mojo/skia/public/mojom/image_info.mojom-webui.js';
import type {Origin} from '//resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {BrowserProxy} from '../browser_proxy.js';
import {ContentSettingsType} from '../content_settings_types.mojom-webui.js';
import type {ActorTaskState as ActorTaskStateMojo, FocusedTabData as FocusedTabDataMojo, GetPinCandidatesOptions as GetPinCandidatesOptionsMojo, GetTabContextOptions as TabContextOptionsMojo, OpenPanelInfo as OpenPanelInfoMojo, OpenSettingsOptions as OpenSettingsOptionsMojo, PanelOpeningData as PanelOpeningDataMojo, PanelState as PanelStateMojo, PinCandidate as PinCandidateMojo, PinCandidatesObserver, ScrollToSelector as ScrollToSelectorMojo, TabContext as TabContextMojo, TabData as TabDataMojo, ViewChangeRequest as ViewChangeRequestMojo, WebClientHandlerInterface, WebClientInterface, ZeroStateSuggestionsOptions as ZeroStateSuggestionsOptionsMojo, ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo} from '../glic.mojom-webui.js';
import {CurrentView as CurrentViewMojo, PinCandidatesObserverReceiver, SettingsPageField as SettingsPageFieldMojo, WebClientHandlerRemote, WebClientMode, WebClientReceiver} from '../glic.mojom-webui.js';
import type {HostCapability as HostCapabilityMojo} from '../glic.mojom-webui.js';
import type {ActorTaskState, DraggableArea, GetPinCandidatesOptions, HostCapability, Journal, OpenSettingsOptions, PageMetadata, PanelOpeningData, PanelState, Screenshot, ScrollToParams, TabContextOptions, ViewChangedNotification, ViewChangeRequest, WebPageData, ZeroStateSuggestions, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../glic_api/glic_api.js';
import {CaptureScreenshotErrorReason, ClientView, CreateTaskErrorReason, DEFAULT_INNER_TEXT_BYTES_LIMIT, DEFAULT_PDF_SIZE_LIMIT, PerformActionsErrorReason, ScrollToErrorReason} from '../glic_api/glic_api.js';
import {ObservableValue} from '../observable.js';
import type {ObservableValueReadOnly} from '../observable.js';
import {OneShotTimer} from '../timer.js';

import {replaceProperties} from './conversions.js';
import type {PostMessageRequestHandler} from './post_message_transport.js';
import {newSenderId, PostMessageRequestReceiver, PostMessageRequestSender, ResponseExtras} from './post_message_transport.js';
import type {AnnotatedPageDataPrivate, FocusedTabDataPrivate, HostRequestTypes, PdfDocumentDataPrivate, RequestRequestType, RequestResponseType, RgbaImage, TabContextResultPrivate, TabDataPrivate, TransferableException, WebClientInitialStatePrivate} from './request_types.js';
import {ErrorWithReasonImpl, exceptionFromTransferable, ImageAlphaType, ImageColorType, requestTypeToHistogramSuffix} from './request_types.js';

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
  MAX_VALUE = UNRESPONSIVE_INACTIVE,
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

// A type which the host should implement. This helps verify that
// `HostMessageHandler` is implemented with the correct parameter and return
// types.
type HostMessageHandlerInterface = {
  [Property in keyof HostRequestTypes]:
      // `payload` is the message payload.
  (payload: RequestRequestType<Property>, extras: ResponseExtras) =>
      Promisify<RequestResponseType<Property>>;
};

class WebClientImpl implements WebClientInterface {
  constructor(
      private sender: PostMessageRequestSender, private host: GlicApiHost,
      private embedder: ApiHostEmbedder) {}

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

  notifyFocusedTabChanged(focusedTabData: (FocusedTabDataMojo)): void {
    const extras = new ResponseExtras();
    this.sender.requestNoResponse(
        'glicWebClientNotifyFocusedTabChanged', {
          focusedTabDataPrivate: focusedTabDataToClient(focusedTabData, extras),
        },
        extras.transfers);
  }
  notifyPanelActiveChange(panelActive: boolean): void {
    this.sender.requestNoResponse(
        'glicWebClientNotifyPanelActiveChanged', {panelActive});
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
    this.sender.requestNoResponse(
        'glicWebClientNotifyPinnedTabsChanged',
        {tabData: tabData.map((x) => tabDataToClient(x, extras))},
        extras.transfers);
  }

  notifyPinnedTabDataChanged(tabData: TabDataMojo): void {
    const extras = new ResponseExtras();
    this.sender.requestNoResponse(
        'glicWebClientNotifyPinnedTabDataChanged',
        {tabData: tabDataToClient(tabData, extras)}, extras.transfers);
  }

  notifyZeroStateSuggestionsChanged(
      suggestions: ZeroStateSuggestionsV2Mojo,
      options: ZeroStateSuggestionsOptionsMojo): void {
    this.sender.requestNoResponse(
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
}

class PinCandidatesObserverImpl implements PinCandidatesObserver {
  constructor(
      private sender: PostMessageRequestSender, public observationId: number) {}

  onPinCandidatesChanged(candidates: PinCandidateMojo[]): void {
    const extras = new ResponseExtras();
    this.sender.requestNoResponse(
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
      private handler: WebClientHandlerInterface,
      private sender: PostMessageRequestSender,
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
    this.receiver = new WebClientReceiver(
        new WebClientImpl(this.sender, this.host, this.embedder));
    const {initialState} = await this.handler.webClientCreated(
        this.receiver.$.bindNewPipeAndPassRemote());
    const chromeVersion = initialState.chromeVersion.components;
    const hostCapabilities = initialState.hostCapabilities;
    this.host.setBrowserIsActive(initialState.browserIsActive);

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

  glicBrowserGetModelQualityClientId(): Promise<{modelQualityClientId: string}> {
    return this.handler.getModelQualityClientId();
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

  async glicBrowserCreateTask(_request: void): Promise<{taskId: number}> {
    try {
      const taskId = await this.handler.createTask();
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

  glicBrowserStopActorTask(request: {taskId: number}): void {
    this.handler.stopActorTask(request.taskId);
  }

  glicBrowserPauseActorTask(request: {taskId: number}): void {
    this.handler.pauseActorTask(request.taskId);
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

  glicBrowserOnResponseStarted(): void {
    this.handler.onResponseStarted();
  }

  glicBrowserOnResponseStopped(): void {
    this.handler.onResponseStopped();
  }

  glicBrowserOnSessionTerminated(): void {
    this.handler.onSessionTerminated();
  }

  glicBrowserOnTurnCompleted(request: {model: number, duration: number}): void {
    this.handler.onTurnCompleted(
        request.model, timeDeltaFromClient(request.duration));
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
    const observer =
        new PinCandidatesObserverImpl(this.sender, request.observationId);
    const receiver = new PinCandidatesObserverReceiver(observer);
    this.host.pinCandidatesObserver = {receiver, observer};
    this.handler.subscribeToPinCandidates(
        getPinCandidatesOptionsFromClient(request.options),
        receiver.$.bindNewPipeAndPassRemote());
  }

  glicBrowserUnsubscribeFromPinCandidates(request: {observationId: number}):
      void {
    if (!this.host.pinCandidatesObserver) {
      return;
    }
    if (this.host.pinCandidatesObserver.observer.observationId ===
        request.observationId) {
      this.host.pinCandidatesObserver.receiver.$.close();
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
  private sender: PostMessageRequestSender;
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
  pinCandidatesObserver?: {
    receiver: PinCandidatesObserverReceiver,
    observer: PinCandidatesObserverImpl,
  };

  constructor(
      private browserProxy: BrowserProxy, private windowProxy: WindowProxy,
      private embeddedOrigin: string, embedder: ApiHostEmbedder) {
    this.postMessageReceiver = new PostMessageRequestReceiver(
        embeddedOrigin, this.senderId, windowProxy, this, 'glic_api_host');
    this.postMessageReceiver.setLoggingEnabled(
        loadTimeData.getBoolean('loggingEnabled'));
    this.sender = new PostMessageRequestSender(
        windowProxy, embeddedOrigin, this.senderId, 'glic_api_host');
    this.sender.setLoggingEnabled(loadTimeData.getBoolean('loggingEnabled'));
    this.handler = new WebClientHandlerRemote();
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
    this.closePinCandidatesObserver();
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
      this.closePinCandidatesObserver();
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

    const response =
        await handlerFunction.call(this.messageHandler, payload, extras);
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
    const suffix = requestTypeToHistogramSuffix(requestType);
    if (suffix === undefined) {
      return;
    }
    chrome.metricsPrivate.recordEnumerationValue(
        `Glic.Api.RequestCounts.${suffix}`, event,
        GlicRequestEvent.MAX_VALUE + 1);
  }

  closePinCandidatesObserver() {
    if (this.pinCandidatesObserver) {
      this.pinCandidatesObserver.receiver.$.close();
      this.pinCandidatesObserver = undefined;
    }
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

function tabIdToClient(tabId: number): string {
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

function urlToClient(url: Url): string {
  return url.url;
}

function urlFromClient(url: string): Url {
  return {url};
}

function originToClient(origin: Origin): string {
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

  let faviconUrl: string|undefined = undefined;
  if (tabData.faviconUrl) {
    faviconUrl = urlToClient(tabData.faviconUrl);
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
    faviconUrl,
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
  };
}

function panelStateToClient(panelState: PanelStateMojo): PanelState {
  return {
    kind: panelState.kind as number,
    windowId: optionalWindowIdToClient(panelState.windowId),
  };
}

/** Takes a time value in milliseconds and converts to a Mojo TimeDelta. */
function timeDeltaFromClient(durationMs: number = 0): TimeDelta {
  if (!Number.isFinite(durationMs)) {
    throw new Error('Invalid duration value: ' + durationMs);
  }
  return {microseconds: BigInt(Math.floor(durationMs * 1000))};
}

function tabContextToClient(
    tabContext: TabContextMojo,
    extras: ResponseExtras): TabContextResultPrivate {
  const tabData = tabContext.tabData;
  let favicon: RgbaImage|undefined = undefined;
  if (tabData.favicon) {
    favicon = bitmapN32ToRGBAImage(tabData.favicon);
    if (favicon) {
      extras.addTransfer(favicon.dataRGBA);
    }
  }

  const tabDataResult: TabDataPrivate = {
    tabId: tabIdToClient(tabData.tabId),
    windowId: windowIdToClient(tabData.windowId),
    url: urlToClient(tabData.url),
    title: optionalToClient(tabData.title),
    favicon,
  };
  const webPageData = tabContext.webPageData;
  let webPageDataResult: WebPageData|undefined = undefined;
  if (webPageData) {
    webPageDataResult = {
      mainDocument: {
        origin: originToClient(webPageData.mainDocument.origin),
        innerText: webPageData.mainDocument.innerText,
        innerTextTruncated: webPageData.mainDocument.innerTextTruncated,
      },
    };
  }
  const viewportScreenshot = tabContext.viewportScreenshot;
  let viewportScreenshotResult: Screenshot|undefined = undefined;
  if (viewportScreenshot) {
    const screenshotArray = new Uint8Array(viewportScreenshot.data);
    viewportScreenshotResult = {
      widthPixels: viewportScreenshot.widthPixels,
      heightPixels: viewportScreenshot.heightPixels,
      data: screenshotArray.buffer,
      mimeType: viewportScreenshot.mimeType,
      originAnnotations: {},
    };
    extras.addTransfer(screenshotArray.buffer);
  }
  let pdfDocumentData: PdfDocumentDataPrivate|undefined = undefined;
  if (tabContext.pdfDocumentData) {
    const pdfData = tabContext.pdfDocumentData.pdfData ?
        new Uint8Array(tabContext.pdfDocumentData.pdfData).buffer :
        undefined;
    if (pdfData) {
      extras.addTransfer(pdfData);
    }
    pdfDocumentData = {
      origin: originToClient(tabContext.pdfDocumentData.origin),
      pdfSizeLimitExceeded: tabContext.pdfDocumentData.sizeLimitExceeded,
      pdfData,
    };
  }
  let annotatedPageData: AnnotatedPageDataPrivate|undefined = undefined;
  if (tabContext.annotatedPageData) {
    const annotatedPageContent =
        tabContext.annotatedPageData.annotatedPageContent ?
        getArrayBufferFromBigBuffer(
            tabContext.annotatedPageData.annotatedPageContent.smuggled) :
        undefined;
    if (annotatedPageContent) {
      extras.addTransfer(annotatedPageContent);
    }
    let metadata: PageMetadata|undefined = undefined;
    if (tabContext.annotatedPageData.metadata) {
      metadata = {
        frameMetadata: tabContext.annotatedPageData.metadata.frameMetadata.map(
          m => replaceProperties(m, {url: urlToClient(m.url)})),
      };
    }
    annotatedPageData = {annotatedPageContent, metadata};
  }

  return {
    tabData: tabDataResult,
    webPageData: webPageDataResult,
    viewportScreenshot: viewportScreenshotResult,
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
