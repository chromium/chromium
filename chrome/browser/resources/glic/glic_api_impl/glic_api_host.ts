// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in
// glic_api_host/glic_api_impl.ts.

// TODO(crbug.com/379677413): Add tests for the API host.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {BitmapN32} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import {AlphaType} from '//resources/mojo/skia/public/mojom/image_info.mojom-webui.js';
import type {Origin} from '//resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {BrowserProxy} from '../browser_proxy.js';
import type {FocusedTabCandidate as FocusedTabCandidateMojo, FocusedTabData as FocusedTabDataMojo, InvalidCandidateError as MojoInvalidCandidateError, NoCandidateTabError as MojoNoCandidateTabError, OpenPanelInfo as OpenPanelInfoMojo, PanelState as PanelStateMojo, ScrollToSelector as ScrollToSelectorMojo, TabData as TabDataMojo, WebClientHandlerInterface, WebClientInterface} from '../glic.mojom-webui.js';
import {WebClientHandlerRemote, WebClientMode, WebClientReceiver} from '../glic.mojom-webui.js';
import type {DraggableArea, PanelState, Screenshot, ScrollToParams, TabContextOptions, WebPageData} from '../glic_api/glic_api.js';
import {CaptureScreenshotErrorReason, DEFAULT_PDF_SIZE_LIMIT, GetTabContextErrorReason, InvalidCandidateError, NoCandidateTabError, ScrollToErrorReason} from '../glic_api/glic_api.js';

import type {PostMessageRequestHandler} from './post_message_transport.js';
import {PostMessageRequestReceiver, PostMessageRequestSender} from './post_message_transport.js';
import type {AnnotatedPageDataPrivate, FocusedTabCandidatePrivate, FocusedTabDataPrivate, HostRequestTypes, PdfDocumentDataPrivate, RgbaImage, TabContextResultPrivate, TabDataPrivate, UserProfileInfoPrivate} from './request_types.js';
import {ErrorWithReasonImpl, ImageAlphaType, ImageColorType} from './request_types.js';

// Implemented by the embedder of GlicApiHost.
export interface ApiHostEmbedder {
  // Called when the guest requests resize.
  onGuestResizeRequest(size: {width: number, height: number}): void;

  // Called after the web client is initialized.
  showGuest(): void;

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
      // `responseTransfer` is populated by objects that should be transferred
      // when sending the message.
  (payload: HostRequestTypes[Property]['request'],
   responseTransfer: Transferable[]) =>
      Promisify<HostRequestTypes[Property]['response']>;
};

class WebClientImpl implements WebClientInterface {
  constructor(
      private sender: PostMessageRequestSender,
      private embedder: ApiHostEmbedder) {}

  notifyPanelOpened(attachedToWindowId: (number|null)): void {
    this.sender.requestNoResponse('glicWebClientNotifyPanelOpened', {
      attachedToWindowId: optionalWindowIdToClient(attachedToWindowId),
    });
  }

  async notifyPanelClosed(): Promise<void> {
    await this.sender.requestWithResponse('glicWebClientNotifyPanelClosed', {});
  }

  async notifyPanelWillOpen(panelState: PanelStateMojo):
      Promise<{openPanelInfo: OpenPanelInfoMojo}> {
    const result = await this.sender.requestWithResponse(
        'glicWebClientNotifyPanelWillOpen',
        {panelState: panelStateToClient(panelState)});

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
    return this.sender.requestWithResponse(
        'glicWebClientNotifyPanelWasClosed', {});
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

  notifyFocusedTabChanged(focusedTabData: (FocusedTabDataMojo)): void {
    const transfer: Transferable[] = [];
    this.sender.requestNoResponse(
        'glicWebClientNotifyFocusedTabChanged', {
          focusedTabDataPrivate:
              focusedTabDataToClient(focusedTabData, transfer),
        },
        transfer);
  }
}

// Handles all requests to the host.
class HostMessageHandler implements HostMessageHandlerInterface {
  // Undefined until the web client is initialized.
  private receiver: WebClientReceiver|undefined;

  constructor(
      private handler: WebClientHandlerInterface,
      private sender: PostMessageRequestSender,
      private embedder: ApiHostEmbedder) {}

  destroy() {
    if (this.receiver) {
      this.receiver.$.close();
    }
  }

  async glicBrowserWebClientCreated({}, transfer: Transferable[]) {
    this.receiver =
        new WebClientReceiver(new WebClientImpl(this.sender, this.embedder));
    const {initialState} = await this.handler.webClientCreated(
        this.receiver.$.bindNewPipeAndPassRemote());
    const chromeVersion = initialState.chromeVersion.components;

    return {
      panelState: panelStateToClient(initialState.panelState),
      focusedTabData:
          focusedTabDataToClient(initialState.focusedTabData, transfer),
      microphonePermissionEnabled: initialState.microphonePermissionEnabled,
      locationPermissionEnabled: initialState.locationPermissionEnabled,
      tabContextPermissionEnabled: initialState.tabContextPermissionEnabled,
      chromeVersion: {
        major: chromeVersion[0] || 0,
        minor: chromeVersion[1] || 0,
        build: chromeVersion[2] || 0,
        patch: chromeVersion[3] || 0,
      },
      canAttach: initialState.canAttach,
      scrollToEnabled: loadTimeData.getBoolean('enableScrollTo'),
    };
  }

  glicBrowserWebClientInitialized(request: {success: boolean}) {
    // The webview may have been re-shown by webui, having previously been
    // opened by the browser. In that case, show the guest frame again.
    this.embedder.showGuest();

    if (request.success) {
      this.handler.webClientInitialized();
    } else {
      this.handler.webClientInitializeFailed();
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

  glicBrowserOpenGlicSettingsPage() {
    this.handler.openGlicSettingsPage();
  }

  glicBrowserClosePanel() {
    return this.handler.closePanel();
  }

  glicBrowserAttachPanel(): void {
    this.handler.attachPanel();
  }

  glicBrowserDetachPanel(): void {
    this.handler.detachPanel();
  }

  glicBrowserShowProfilePicker() {
    return this.handler.showProfilePicker();
  }

  async glicBrowserGetContextFromFocusedTab(
      request: {options: TabContextOptions}, transfer: Transferable[]):
      Promise<{tabContextResult: TabContextResultPrivate}> {
    const {
      result: {errorReason, tabContext},
    } = await this.handler.getContextFromFocusedTab({
      includeInnerText: request.options.innerText || false,
      includeViewportScreenshot: request.options.viewportScreenshot || false,
      includePdf: request.options.pdfData || false,
      includeAnnotatedPageContent:
          request.options.annotatedPageContent || false,
      pdfSizeLimit: request.options.pdfSizeLimit === undefined ?
          DEFAULT_PDF_SIZE_LIMIT :
          Math.min(Number.MAX_SAFE_INTEGER, request.options.pdfSizeLimit),
    });
    if (!tabContext) {
      throw new ErrorWithReasonImpl(
          'tabContext',
          (errorReason as GetTabContextErrorReason | undefined) ??
              GetTabContextErrorReason.UNKNOWN);
    }
    const tabData = tabContext.tabData;
    let favicon: RgbaImage|undefined = undefined;
    if (tabData.favicon) {
      favicon = bitmapN32ToRGBAImage(tabData.favicon);
      if (favicon) {
        transfer.push(favicon.dataRGBA);
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
      transfer.push(screenshotArray.buffer);
    }
    let pdfDocumentData: PdfDocumentDataPrivate|undefined = undefined;
    if (tabContext.pdfDocumentData) {
      const pdfData = tabContext.pdfDocumentData.pdfData ?
          new Uint8Array(tabContext.pdfDocumentData.pdfData).buffer :
          undefined;
      if (pdfData) {
        transfer.push(pdfData);
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
        transfer.push(annotatedPageContent);
      }
      annotatedPageData = {annotatedPageContent};
    }

    return {
      tabContextResult: {
        tabData: tabDataResult,
        webPageData: webPageDataResult,
        viewportScreenshot: viewportScreenshotResult,
        pdfDocumentData,
        annotatedPageData,
      },
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

  async glicBrowserCaptureScreenshot(_request: {}, transfer: Transferable[]):
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
    transfer.push(screenshotArray.buffer);
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

  glicBrowserSetMicrophonePermissionState(request: {enabled: boolean}) {
    return this.handler.setMicrophonePermissionState(request.enabled);
  }

  glicBrowserSetLocationPermissionState(request: {enabled: boolean}) {
    return this.handler.setLocationPermissionState(request.enabled);
  }

  glicBrowserSetTabContextPermissionState(request: {enabled: boolean}) {
    return this.handler.setTabContextPermissionState(request.enabled);
  }

  async glicBrowserGetUserProfileInfo(_request: {}, transfer: Transferable[]) {
    const {profileInfo: mojoProfileInfo} =
        await this.handler.getUserProfileInfo();
    if (!mojoProfileInfo) {
      return {};
    }
    const {displayName, email, avatarIcon} = mojoProfileInfo;
    const profileInfo: UserProfileInfoPrivate = {displayName, email};
    if (avatarIcon) {
      profileInfo.avatarIcon = bitmapN32ToRGBAImage(avatarIcon);
      if (profileInfo.avatarIcon) {
        transfer.push(profileInfo.avatarIcon.dataRGBA);
      }
    }
    return {profileInfo};
  }

  glicBrowserRefreshSignInCookies(): Promise<{success: boolean}> {
    return this.handler.syncCookies();
  }

  glicBrowserSetContextAccessIndicator(request: {show: boolean}) {
    this.handler.setContextAccessIndicator(request.show);
  }

  glicBrowserSetAudioDucking(request: {enabled: boolean}) {
    this.handler.setAudioDucking(request.enabled);
  }

  glicBrowserOnUserInputSubmitted(request: {mode: number}) {
    this.handler.onUserInputSubmitted(request.mode);
  }

  glicBrowserOnResponseStarted() {
    this.handler.onResponseStarted();
  }

  glicBrowserOnResponseStopped() {
    this.handler.onResponseStopped();
  }

  glicBrowserOnSessionTerminated() {
    this.handler.onSessionTerminated();
  }

  glicBrowserOnResponseRated(request: {positive: boolean}) {
    this.handler.onResponseRated(request.positive);
  }

  async glicBrowserScrollTo(request: {params: ScrollToParams}) {
    const {params} = request;

    function getMojoSelector(): ScrollToSelectorMojo {
      const {selector} = params;
      if (selector.exactText !== undefined) {
        return {
          exactTextSelector: {
            text: selector.exactText!.text,
          },
        };
      }
      if (selector.textFragment !== undefined) {
        return {
          textFragmentSelector: {
            textStart: selector.textFragment!.textStart,
            textEnd: selector.textFragment!.textEnd,
          },
        };
      }
      throw new ErrorWithReasonImpl(
          'scrollTo', ScrollToErrorReason.NOT_SUPPORTED);
    }

    const mojoParams = {
      highlight: params.highlight === undefined ? true : params.highlight,
      selector: getMojoSelector(),
    };
    const {errorReason} = (await this.handler.scrollTo(mojoParams));
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl('scrollTo', errorReason! as number);
    }
    return {};
  }
}

export class GlicApiHost implements PostMessageRequestHandler {
  private messageHandler: HostMessageHandler;
  private readonly postMessageReceiver: PostMessageRequestReceiver;
  private sender: PostMessageRequestSender;
  private handler: WebClientHandlerRemote;
  private bootstrapPingIntervalId: number|undefined;

  constructor(
      private browserProxy: BrowserProxy, private windowProxy: WindowProxy,
      private embeddedOrigin: string, embedder: ApiHostEmbedder) {
    this.postMessageReceiver =
        new PostMessageRequestReceiver(embeddedOrigin, windowProxy, this);
    this.sender = new PostMessageRequestSender(windowProxy, embeddedOrigin);
    this.handler = new WebClientHandlerRemote();
    this.browserProxy.handler.createWebClient(
        this.handler.$.bindNewPipeAndPassReceiver());
    this.messageHandler =
        new HostMessageHandler(this.handler, this.sender, embedder);

    this.bootstrapPingIntervalId =
        window.setInterval(this.bootstrapPing.bind(this), 50);
    this.bootstrapPing();
  }

  destroy() {
    window.clearInterval(this.bootstrapPingIntervalId);
    this.postMessageReceiver.destroy();
    this.messageHandler.destroy();
    this.sender.destroy();
  }

  // Called when the webview page is loaded.
  contentLoaded() {
    // Send the ping message one more time. At this point, the webview should
    // be able to handle the message, if it hasn't already.
    this.bootstrapPing();
    this.stopBootstrapPing();
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

  async openLinkInNewTab(url: string) {
    await this.handler.createTab(urlFromClient(url), false, null);
  }

  // PostMessageRequestHandler implementation.
  async handleRawRequest(type: string, payload: any):
      Promise<{payload: any, transfer: Transferable[]}|undefined> {
    const handlerFunction = (this.messageHandler as any)[type];
    if (typeof handlerFunction !== 'function') {
      console.error(`GlicApiHost: Unknown message type ${type}`);
      return;
    }

    this.stopBootstrapPing();
    const transfer: Transferable[] = [];
    const response =
        await handlerFunction.call(this.messageHandler, payload, transfer);
    if (!response) {
      // Not all request types require a return value.
      return;
    }
    return {payload: response, transfer};
  }
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

function tabDataToClient(tabData: TabDataMojo|null, transfer: Transferable[]):
    TabDataPrivate|undefined {
  if (!tabData) {
    return undefined;
  }

  let favicon: RgbaImage|undefined = undefined;
  if (tabData.favicon) {
    favicon = bitmapN32ToRGBAImage(tabData.favicon);
    if (favicon) {
      transfer.push(favicon.dataRGBA);
    }
  }

  return {
    tabId: tabIdToClient(tabData.tabId),
    windowId: windowIdToClient(tabData.windowId),
    url: urlToClient(tabData.url),
    title: optionalToClient(tabData.title),
    favicon,
    documentMimeType: tabData.documentMimeType,
  };
}

function focusedTabCandidateToClient(
    focusedTabCandidate: FocusedTabCandidateMojo,
    transfer: Transferable[]): FocusedTabCandidatePrivate {
  const focusedTabCandidateData =
      tabDataToClient(focusedTabCandidate.focusedTabCandidateData, transfer);
  const invalidCandidateError =
      invalidCandidateErrorToClient(focusedTabCandidate.invalidCandidateError);
  return {
    focusedTabCandidateData,
    invalidCandidateError,
  };
}

function focusedTabDataToClient(
    focusedTabData: FocusedTabDataMojo,
    transfer: Transferable[]): FocusedTabDataPrivate {
  if (focusedTabData.focusedTab) {
    return {
      focusedTab: tabDataToClient(focusedTabData.focusedTab, transfer),
    };
  }
  if (focusedTabData.focusedTabCandidate) {
    return {
      focusedTabCandidate: focusedTabCandidateToClient(
          focusedTabData.focusedTabCandidate, transfer),
    };
  }
  if (focusedTabData.noCandidateTabError) {
    return {
      noCandidateTabError:
          noCandidateTabErrorToClient(focusedTabData.noCandidateTabError),
    };
  }
  return {noCandidateTabError: NoCandidateTabError.UNKNOWN};
}

function invalidCandidateErrorToClient(
    mojoReason: MojoInvalidCandidateError|null): InvalidCandidateError|
    undefined {
  if (!mojoReason) {
    return undefined;
  }
  return (mojoReason.valueOf() as InvalidCandidateError | undefined) ??
      InvalidCandidateError.UNKNOWN;
}

function noCandidateTabErrorToClient(mojoReason: MojoNoCandidateTabError|null):
    NoCandidateTabError|undefined {
  if (!mojoReason) {
    return undefined;
  }
  return (mojoReason.valueOf() as NoCandidateTabError) ??
      NoCandidateTabError.UNKNOWN;
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
