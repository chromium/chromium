// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in ../client/.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxy} from '../../browser_proxy.js';
import {ActorClientReceiver, ActorHandlerRemote, WebClientHandlerRemote} from '../../glic.mojom-webui.js';
import type {ExperimentalTriggeringUpdatesHandlerRemote, WebClientInitialState} from '../../glic.mojom-webui.js';
import type {ClientCapabilities} from '../../glic_api/glic_api.js';
import {ObservableValue} from '../../observable.js';
import type {ObservableValueReadOnly} from '../../observable.js';
import {TaskQueue} from '../../task_queue.js';
import {OneShotTimer} from '../../timer.js';
import {ActorHostMessageHandler} from '../actor/actor_host.js';
import {ActorClientDef, ActorHostDef} from '../actor/actor_types.js';
import type {ResponseExtras} from '../transport/messaging.js';
import type {InterfaceDef, PendingReceiver, PendingRemote, PostMessageHandler, PostMessageLifecycleObserver, PostMessageReceiver, PostMessageRemote, PostMessageRequestReceiver, PostMessageRequestSender, PostMessageRouter} from '../transport/post_message_transport.js';
import {createBidirectionalPostMessageTransport} from '../transport/post_message_transport.js';

import {ERROR_CODEC, getHostRequestHistogramInfo, MAX_REQUEST_ID, WebClientDef, WebClientHostDef} from './../request_types.js';
import type {ActorClient, ActorHost, WebClient, WebClientHost} from './../request_types.js';
import {urlFromClient} from './conversions.js';
import {GatedSender} from './gated_sender.js';
import {HostMessageHandler, TabDataHandlerSet, TabFaviconHandlerSet} from './host_from_client.js';
import type {CaptureRegionObserverImpl, PinCandidatesObserverImpl} from './host_from_client.js';
import {ActorClientImpl} from './host_to_client.js';
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
  // OBSOLETE: MOJO_PIPE_CLOSED_UNEXPECTEDLY = 9,
  MOJO_PIPE_CLOSED_UNEXPECTEDLY_BEFORE_INITIALIZE = 10,
  MOJO_PIPE_CLOSED_UNEXPECTEDLY_AFTER_INITIALIZE = 11,
  MAX_VALUE = MOJO_PIPE_CLOSED_UNEXPECTEDLY_AFTER_INITIALIZE,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicDetailedWebClientState)

// Implemented by the embedder of GlicApiHost.
export interface ApiHostEmbedder {
  // Called when the guest requests to enable manual drag resize.
  enableDragResize(enabled: boolean): void;

  // Called when the notifyPanelWillOpen promise resolves to open the panel
  // when triggered from the browser.
  webClientReady(): void;
  webClientWarmed(): void;

  // Returns the current zoom level of the webview.
  getZoom(): Promise<number>;
}


// Sets up communication with the client.
// This is separate from GlicApiHost to allow us to detect the client page
// before our host is really ready to connect.
export class GlicApiCommunicator implements PostMessageLifecycleObserver {
  readonly postMessageReceiver: PostMessageRequestReceiver;
  readonly postMessageSender: PostMessageRequestSender;
  readonly pmRemote: PostMessageRemote<WebClient>;
  readonly router: PostMessageRouter;
  private bootstrapPingIntervalId: number|undefined;
  private loggingEnabled = loadTimeData.getBoolean('loggingEnabled');
  private host?: GlicApiHost;
  private hostPromise = Promise.withResolvers<GlicApiHost>();
  private rootReceiver: PostMessageReceiver;

  constructor(
      private embeddedOrigin: string, private windowProxy: WindowProxy) {
    const {router, sender, receiver, rootRemote, rootReceiver} =
        createBidirectionalPostMessageTransport(
            embeddedOrigin, windowProxy, this,
            this as unknown as PostMessageHandler<WebClientHost>,
            'glic_api_host', true, ERROR_CODEC, WebClientHostDef, WebClientDef);
    this.rootReceiver = rootReceiver;
    this.pmRemote = rootRemote;
    this.router = router;
    this.postMessageReceiver = receiver;
    this.postMessageSender = sender;
    this.router.setLoggingEnabled(this.loggingEnabled);
    this.postMessageSender.setMaxInFlightRequests(
        loadTimeData.getInteger('maxInFlightRequests'));
    this.postMessageSender.sendResponsesForAllRequests =
        loadTimeData.getBoolean('sendResponsesForAllRequests');

    this.bootstrapPingIntervalId =
        window.setInterval(this.bootstrapPing.bind(this), 50);
    this.bootstrapPing();
  }

  destroy() {
    window.clearInterval(this.bootstrapPingIntervalId);
    this.router.destroy();
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

  // Intercept initial handshake on pipe 0.
  async webClientCreated(
      payload: {clientCapabilities: ClientCapabilities[]},
      extras: ResponseExtras) {
    this.stopBootstrapPing();
    const h = await this.hostPromise.promise;
    this.postMessageReceiver.requestObserver = h;
    this.postMessageReceiver.setHandlerWrapper(h.handlerWrapper.bind(h));
    this.rootReceiver.setMessageHandler(h.hostMessageHandler, WebClientHostDef);
    const handleFn =
        (h.hostMessageHandler as unknown as
         Record<string, HandlerFunction>)['webClientCreated'];
    if (!handleFn) {
      return undefined;
    }
    return await handleFn.call(h.hostMessageHandler, payload, extras);
  }
  // Just ignore message callbacks before the host is connected.
  onRequestReceived(_type: string, _interfaceDef: InterfaceDef|undefined):
      void {}
  onRequestHandlerException(
      _type: string, _interfaceDef: InterfaceDef|undefined): void {}
  onRequestCompleted(_type: string, _interfaceDef: InterfaceDef|undefined):
      void {}

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


type HandlerFunction = (payload: unknown, extras: ResponseExtras) =>
    Promise<unknown>;

/**
 * The host side of the Glic API.
 *
 * Its primary job is to route calls between the client (over postMessage) and
 * the browser (over Mojo).
 */
export class GlicApiHost implements PostMessageLifecycleObserver {
  hostMessageHandler: HostMessageHandler;
  sender: GatedSender<WebClient>;
  actorSender?: GatedSender<ActorClient>;
  private readonly apiGatingOn = ObservableValue.withValue(false);
  private enableApiActivationGating = true;
  panelIsActive = false;
  private isInvoking = false;
  private handler: WebClientHandlerRemote;
  private webClientErrorTimer: OneShotTimer;
  private webClientState =
      ObservableValue.withValue<WebClientState>(WebClientState.UNINITIALIZED);
  openCloseTasks = new TaskQueue();
  private waitingOnPanelWillOpenValue = false;
  // Synchronizes panel open/close events between the browser and client,
  // ensuring panel open state is eventually consistent.
  private clientActiveObs = ObservableValue.withValue(false);
  // The open state as understood by the client, this is delayed
  // from the notifyPanelWillOpen and notifyPanelWasClosed calls because
  // processing is async.
  private panelOpenState = PanelOpenState.CLOSED;
  private instanceIsActive = true;
  private hasShownDebuggerAttachedWarning = false;
  private loggingEnabled = loadTimeData.getBoolean('loggingEnabled');
  detailedWebClientState = DetailedWebClientState.BOOTSTRAP_PENDING;
  // Present while the client is monitoring pin candidates.
  pinCandidatesObserver?: PinCandidatesObserverImpl;
  captureRegionObserver?: CaptureRegionObserverImpl;
  tabDataHandlerSet: TabDataHandlerSet;
  tabFaviconHandlerSet: TabFaviconHandlerSet;

  actorHandler?: ActorHandlerRemote;
  private isSubscribedToZoomLevel = false;
  private experimentalTriggeringUpdatesHandler =
      new Map<number, ExperimentalTriggeringUpdatesHandlerRemote>();
  private nextExperimentalTriggeringUpdateHandlerId = 0;

  constructor(
      private browserProxy: BrowserProxy,
      public readonly communicator: GlicApiCommunicator,
      private embedder: ApiHostEmbedder) {
    this.sender = new GatedSender(communicator.pmRemote, this.apiGatingOn);
    this.handler = new WebClientHandlerRemote();
    this.handler.onConnectionError.addListener(() => {
      if (this.webClientState.getCurrentValue() !== WebClientState.ERROR) {
        console.warn(`Mojo connection error in glic host`);
        this.detailedWebClientState = this.detailedWebClientState ===
                DetailedWebClientState.BOOTSTRAP_PENDING ?
            DetailedWebClientState
                .MOJO_PIPE_CLOSED_UNEXPECTEDLY_BEFORE_INITIALIZE :
            DetailedWebClientState
                .MOJO_PIPE_CLOSED_UNEXPECTEDLY_AFTER_INITIALIZE;
        this.webClientState.assignAndSignal(WebClientState.ERROR);
      }
    });
    this.handler.$.close();
    this.tabDataHandlerSet =
        new TabDataHandlerSet(communicator.pmRemote, this.handler);
    this.tabFaviconHandlerSet =
        new TabFaviconHandlerSet(communicator.pmRemote, this.handler);

    this.browserProxy.pageHandler.createWebClient(
        this.handler.$.bindNewPipeAndPassReceiver());
    this.hostMessageHandler =
        new HostMessageHandler(this.handler, this.sender, embedder, this);
    this.webClientErrorTimer = new OneShotTimer(
        loadTimeData.getInteger('clientUnresponsiveUiMaxTimeMs'));

    communicator.setHost(this);
  }

  destroy() {
    this.webClientState = ObservableValue.withValue<WebClientState>(
        WebClientState.ERROR);  // Final state
    this.webClientErrorTimer.reset();
    this.hostMessageHandler.destroy();
    this.pinCandidatesObserver?.disconnectFromSource();
    this.captureRegionObserver?.disconnectFromSource();
    if (this.actorHandler) {
      this.actorHandler.$.close();
      this.actorHandler = undefined;
    }
    for (const handler of this.experimentalTriggeringUpdatesHandler.values()) {
      handler.$.close();
    }
    this.experimentalTriggeringUpdatesHandler.clear();
  }

  setInitialState(initialState: WebClientInitialState): {
    actorRemote?: PendingRemote<ActorHost>,
    actorReceiver?: PendingReceiver<ActorClient>,
  } {
    this.enableApiActivationGating = initialState.enableApiActivationGating;
    this.panelIsActive = initialState.panelIsActive;

    this.updateSenderActive();

    if (!initialState.enableActInFocusedTab) {
      return {};
    }
    this.actorHandler = new ActorHandlerRemote();
    const {remote: clientRemote, receiver: actorReceiver} =
        this.communicator.router.newPipeWithRemote(ActorClientDef);
    this.actorSender = new GatedSender(clientRemote, this.apiGatingOn);
    const actorClientReceiver =
        new ActorClientReceiver(new ActorClientImpl(this.actorSender));
    this.handler.createActorHandler(
        this.actorHandler.$.bindNewPipeAndPassReceiver(),
        actorClientReceiver.$.bindNewPipeAndPassRemote());
    const actorHostMessageHandler =
        new ActorHostMessageHandler(this.actorHandler);
    const {remote: actorRemote /* receiver never closed */} =
        this.communicator.router.newPipeWithReceiver(
            actorHostMessageHandler, ActorHostDef);

    return {
      actorRemote,
      actorReceiver,
    };
  }

  updateSenderActive() {
    const shouldGate = this.shouldGateRequests();
    if (this.sender.isGating() === shouldGate) {
      return;
    }
    this.apiGatingOn.assignAndSignal(shouldGate);
  }

  shouldGateRequests(): boolean {
    if (this.isInvoking) {
      return false;
    }
    return !this.panelIsActive && this.enableApiActivationGating;
  }

  async subscribeToZoomLevel() {
    this.isSubscribedToZoomLevel = true;
    try {
      const zoomFactor = await this.embedder.getZoom();
      this.sender.sendLatestWhenActive(
          'glicWebClientNotifyZoomLevelChanged', {zoomFactor});
    } catch (e) {
      console.warn('Failed to get initial zoom level', e);
    }
  }

  unsubscribeFromZoomLevel() {
    this.isSubscribedToZoomLevel = false;
  }

  onZoomLevelChanged(zoomFactor: number) {
    if (this.isSubscribedToZoomLevel) {
      this.sender.sendLatestWhenActive(
          'glicWebClientNotifyZoomLevelChanged', {zoomFactor});
    }
  }

  setIsInvoking(isInvoking: boolean) {
    this.isInvoking = isInvoking;
    this.updateSenderActive();
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
      const SMALL_QUEUE_SIZE = 50;
      const hostSendMessageQueueLength =
          this.sender.getRawSender().rawSender().messageQueueLength() +
          this.sender.getRawSender().rawSender().inFlightRequestCount();
      if (hostSendMessageQueueLength >= SMALL_QUEUE_SIZE) {
        chrome.histograms.recordMediumCount(
            'Glic.Host.HostSendMessageQueueLength', hostSendMessageQueueLength);
      }

      let gotResponse = false;
      const responsePromise =
          this.sender
              .requestWithResponse('glicWebClientCheckResponsive', undefined)
              .then((response: {clientSendMessageQueueLength: number}) => {
                gotResponse = true;
                if (response.clientSendMessageQueueLength >= SMALL_QUEUE_SIZE) {
                  chrome.histograms.recordMediumCount(
                      'Glic.Host.ClientSendMessageQueueLength',
                      response.clientSendMessageQueueLength);
                }
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

  async handlerWrapper(
      type: string, interfaceDef: InterfaceDef|undefined, payload: unknown,
      extras: ResponseExtras,
      handlerFunction: HandlerFunction): Promise<unknown> {
    if (this.detailedWebClientState ===
        DetailedWebClientState.BOOTSTRAP_PENDING) {
      this.detailedWebClientState =
          DetailedWebClientState.WEB_CLIENT_NOT_CREATED;
    }

    let response;
    if (this.shouldGateRequests() &&
        Object.hasOwn(BACKGROUND_RESPONSES, type)) {
      if (this.loggingEnabled) {
        console.warn(`GlicApiHost: Using background behavior for ${type}`);
      }
      const backgroundResponse =
          BACKGROUND_RESPONSES[type as keyof typeof BACKGROUND_RESPONSES] as
          HostBackgroundResponse<unknown>;
      if (Object.hasOwn(backgroundResponse, 'throws')) {
        const friendlyName = type.replaceAll(/^glicWebClient/g, '');
        throw new Error(`${friendlyName} not allowed while backgrounded`);
      }
      if (this.loggingEnabled) {
        console.warn(`Using background request behavior for ${type}`);
      }
      if (Object.hasOwn(backgroundResponse, 'does')) {
        response =
            await (backgroundResponse as HostBackgroundResponseDoes<unknown>)
                .does();
      } else {
        response =
            (backgroundResponse as HostBackgroundResponseReturns<unknown>)
                .returns;
      }
    } else {
      // Request is not gated, so call the handler directly.
      const startTime = performance.now();
      response = await handlerFunction(payload, extras);
      if (response) {
        // Report latency metric for handled requests that return a response.
        const latency = performance.now() - startTime;
        this.reportLatency(type, interfaceDef, latency);
      }
    }
    // Not all request types require a return value.
    return response;
  }

  onRequestReceived(type: string, interfaceDef: InterfaceDef|undefined): void {
    this.reportRequestCountEvent(
        type, interfaceDef, GlicRequestEvent.REQUEST_RECEIVED);
    if (!this.panelIsActive) {
      this.reportRequestCountEvent(
          type, interfaceDef, GlicRequestEvent.REQUEST_RECEIVED_WHILE_INACTIVE);
    }
  }

  onRequestHandlerException(type: string, interfaceDef: InterfaceDef|undefined):
      void {
    this.reportRequestCountEvent(
        type, interfaceDef, GlicRequestEvent.REQUEST_HANDLER_EXCEPTION);
  }

  onRequestCompleted(type: string, interfaceDef: InterfaceDef|undefined): void {
    this.reportRequestCountEvent(
        type, interfaceDef, GlicRequestEvent.RESPONSE_SENT);
  }

  reportRequestCountEvent(
      requestType: string, interfaceDef: InterfaceDef|undefined,
      event: GlicRequestEvent) {
    const histogramInfo =
        getHostRequestHistogramInfo(requestType, interfaceDef);
    if (histogramInfo === undefined) {
      return;
    }
    chrome.histograms.recordEnumerationValue(
        `Glic.Api.RequestCounts.${histogramInfo.name}`, event,
        GlicRequestEvent.MAX_VALUE + 1);

    switch (event) {
      case GlicRequestEvent.REQUEST_HANDLER_EXCEPTION:
        chrome.histograms.recordEnumerationValue(
            `Glic.Api.StatusCounts.Error`, histogramInfo.id,
            MAX_REQUEST_ID + 1);
        break;
      case GlicRequestEvent.REQUEST_RECEIVED_WHILE_INACTIVE:
        chrome.histograms.recordEnumerationValue(
            `Glic.Api.StatusCounts.Inactive`, histogramInfo.id,
            MAX_REQUEST_ID + 1);
        break;
      case GlicRequestEvent.REQUEST_RECEIVED:
        chrome.histograms.recordEnumerationValue(
            `Glic.Api.StatusCounts.Received`, histogramInfo.id,
            MAX_REQUEST_ID + 1);
        break;
      default:
        break;
    }
  }

  addExperimentalTriggeringUpdatesHandler(
      handler: ExperimentalTriggeringUpdatesHandlerRemote): number {
    const id = this.nextExperimentalTriggeringUpdateHandlerId++;
    this.experimentalTriggeringUpdatesHandler.set(id, handler);
    return id;
  }

  getExperimentalTriggeringUpdatesHandler(observationId: number):
      ExperimentalTriggeringUpdatesHandlerRemote|undefined {
    return this.experimentalTriggeringUpdatesHandler.get(observationId);
  }

  deleteExperimentalTriggeringUpdatesHandler(observationId: number): void {
    this.experimentalTriggeringUpdatesHandler.delete(observationId);
  }

  reportLatency(
      requestType: string, interfaceDef: InterfaceDef|undefined,
      latencyMs: number) {
    const histogramInfo =
        getHostRequestHistogramInfo(requestType, interfaceDef);
    if (histogramInfo === undefined) {
      return;
    }
    chrome.histograms.recordTime(
        `Glic.Api.RequestHostLatency.${histogramInfo.name}`,
        Math.round(latencyMs));
  }
}

// LINT.IfChange(GlicRequestEvent)
enum GlicRequestEvent {
  REQUEST_RECEIVED = 0,
  RESPONSE_SENT = 1,
  REQUEST_HANDLER_EXCEPTION = 2,
  // Deprecated: REQUEST_RECEIVED_WHILE_HIDDEN = 3,
  REQUEST_RECEIVED_WHILE_INACTIVE = 4,
  MAX_VALUE = REQUEST_RECEIVED_WHILE_INACTIVE,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicRequestEvent)

// Returns a Promise resolving after 'ms' milliseconds
function sleep(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms));
}
