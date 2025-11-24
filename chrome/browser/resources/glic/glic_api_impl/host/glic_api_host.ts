// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in ../client/.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxy} from '../../browser_proxy.js';
import type {WebClientInitialState} from '../../glic.mojom-webui.js';
import {WebClientHandlerRemote} from '../../glic.mojom-webui.js';
import {ObservableValue} from '../../observable.js';
import type {ObservableValueReadOnly} from '../../observable.js';
import {OneShotTimer} from '../../timer.js';

import type {PostMessageRequestHandler, ResponseExtras} from './../post_message_transport.js';
import {newSenderId, PostMessageRequestReceiver, PostMessageRequestSender} from './../post_message_transport.js';
import {HOST_REQUEST_TYPES, requestTypeToHistogramSuffix} from './../request_types.js';
import {urlFromClient} from './conversions.js';
import {GatedSender} from './gated_sender.js';
import type {CaptureRegionObserverImpl, PinCandidatesObserverImpl} from './host_from_client.js';
import {HostMessageHandler} from './host_from_client.js';
import type {HostBackgroundResponse, HostBackgroundResponseDoes, HostBackgroundResponseReturns} from './types.js';
import {BACKGROUND_RESPONSES} from './types.js';

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


// Sets up communication with the client.
// This is separate from GlicApiHost to allow us to detect the client page
// before our host is really ready to connect.
export class GlicApiCommunicator implements PostMessageRequestHandler {
  private senderId = newSenderId();
  readonly postMessageReceiver: PostMessageRequestReceiver;
  readonly postMessageSender: PostMessageRequestSender;
  private bootstrapPingIntervalId: number|undefined;
  private loggingEnabled = loadTimeData.getBoolean('loggingEnabled');
  private host?: GlicApiHost;
  private hostPromise = Promise.withResolvers<GlicApiHost>();

  constructor(
      private embeddedOrigin: string, private windowProxy: WindowProxy) {
    this.postMessageReceiver = new PostMessageRequestReceiver(
        embeddedOrigin, this.senderId, windowProxy, this, 'glic_api_host');
    this.postMessageReceiver.setLoggingEnabled(this.loggingEnabled);
    this.postMessageSender = new PostMessageRequestSender(
        windowProxy, embeddedOrigin, this.senderId, 'glic_api_host');
    this.postMessageSender.setLoggingEnabled(this.loggingEnabled);

    this.bootstrapPingIntervalId =
        window.setInterval(this.bootstrapPing.bind(this), 50);
    this.bootstrapPing();
  }

  destroy() {
    window.clearInterval(this.bootstrapPingIntervalId);
    this.postMessageReceiver.destroy();
    this.postMessageSender.destroy();
  }

  // Should be called only once.
  setHost(host: GlicApiHost) {
    assert(!this.host);
    this.host = host;
    this.hostPromise.resolve(host);
  }

  // Called when the webview page is loaded.
  contentLoaded() {
    // Send the ping message one more time. At this point, the webview should
    // be able to handle the message, if it hasn't already.
    this.bootstrapPing();
    this.stopBootstrapPing();
  }

  // PostMessageRequestHandler impl.
  handleRawRequest(type: string, payload: any, extras: ResponseExtras):
      Promise<{payload: any}|undefined> {
    this.stopBootstrapPing();

    if (type === 'glicBrowserWebClientCreated') {
      return this.hostPromise.promise.then(h => {
        // Pass message handling to the host, only after bootstrapping
        // is done.
        this.postMessageReceiver.handler = h;
        return h.handleRawRequest(type, payload, extras);
      });
    } else {
      return Promise.resolve(undefined);
    }
  }
  // Just ignore message callbacks before the host is connected.
  onRequestReceived(_type: string): void {}
  onRequestHandlerException(_type: string): void {}
  onRequestCompleted(_type: string): void {}

  private stopBootstrapPing() {
    if (this.bootstrapPingIntervalId !== undefined) {
      window.clearInterval(this.bootstrapPingIntervalId);
      this.bootstrapPingIntervalId = undefined;
    }
  }

  // Sends a message to the webview which is required to initialize the client.
  // Because we don't know when the client will be ready to receive this
  // message, we start sending this every 50ms as soon as navigation commits on
  // the webview, and stop sending this when the page loads, or we receive a
  // request from the client.
  private bootstrapPing() {
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
}

/**
 * The host side of the Glic API.
 *
 * Its primary job is to route calls between the client (over postMessage) and
 * the browser (over Mojo).
 */
export class GlicApiHost implements PostMessageRequestHandler {
  private messageHandler: HostMessageHandler;
  sender: GatedSender;
  private enableApiActivationGating = true;
  panelIsActive = false;
  private handler: WebClientHandlerRemote;
  private webClientErrorTimer: OneShotTimer;
  private webClientState =
      ObservableValue.withValue<WebClientState>(WebClientState.UNINITIALIZED);
  private waitingOnPanelWillOpenValue = false;
  private clientActiveObs = ObservableValue.withValue(false);
  private panelOpenState = PanelOpenState.CLOSED;
  private instanceIsActive = true;
  private hasShownDebuggerAttachedWarning = false;
  private loggingEnabled = loadTimeData.getBoolean('loggingEnabled');
  detailedWebClientState = DetailedWebClientState.BOOTSTRAP_PENDING;
  // Present while the client is monitoring pin candidates.
  pinCandidatesObserver?: PinCandidatesObserverImpl;
  captureRegionObserver?: CaptureRegionObserverImpl;

  constructor(
      private browserProxy: BrowserProxy, communicator: GlicApiCommunicator,
      embedder: ApiHostEmbedder) {
    this.sender = new GatedSender(communicator.postMessageSender);
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

    communicator.setHost(this);
  }

  destroy() {
    this.webClientState = ObservableValue.withValue<WebClientState>(
        WebClientState.ERROR);  // Final state
    this.webClientErrorTimer.reset();
    this.messageHandler.destroy();
    this.pinCandidatesObserver?.disconnectFromSource();
    this.captureRegionObserver?.destroy();
  }

  setInitialState(initialState: WebClientInitialState) {
    this.enableApiActivationGating = initialState.enableApiActivationGating;
    this.panelIsActive = initialState.panelIsActive;
    this.updateSenderActive();
  }

  updateSenderActive() {
    const shouldGate = this.shouldGateRequests();
    if (this.sender.isGating() === shouldGate) {
      return;
    }

    if (shouldGate) {
      // Becoming inactive, cancel capture.
      this.captureRegionObserver?.destroy();
      this.captureRegionObserver = undefined;
    }
    this.sender.setGating(shouldGate);
  }

  shouldGateRequests(): boolean {
    return !this.panelIsActive && this.enableApiActivationGating;
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
      this.captureRegionObserver?.destroy();
      this.captureRegionObserver = undefined;
    } else {
      this.pinCandidatesObserver?.connectToSource();
    }
  }

  setInstanceIsActive(instanceIsActive: boolean) {
    this.instanceIsActive = instanceIsActive;
    this.clientActiveObs.assignAndSignal(this.isClientActive());
  }

  // Returns true if the user might be interacting with the client.
  // That is, the panel is open, not in an error state, and either the panel
  // itself is focused or a browser window it could be accessing is.
  private isClientActive() {
    return this.panelOpenState === PanelOpenState.OPEN &&
        this.webClientState.getCurrentValue() !== WebClientState.ERROR &&
        this.instanceIsActive;
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

  openLinkInPopup(url: string, initialWidth: number, initialHeight: number) {
    this.handler.openLinkInPopup(
        urlFromClient(url), initialWidth, initialHeight);
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
      if (this.loggingEnabled) {
        console.warn(`Using background request behavior for ${type}`);
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
