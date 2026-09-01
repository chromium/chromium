// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome-WebUI-side of the Glic API.
// Communicates with the web client side in ../client/.

import {assert} from '//resources/js/assert.js';

import {ActorClientReceiver, ActorHandlerRemote, AnnotationHandlerRemote, ExperimentalTriggeringClientReceiver, GlicRequestEvent as MojomGlicRequestEvent, WebClientHandlerRemote, ZeroStateSuggestionsHandlerRemote} from '../../glic.mojom-webui.js';
import type {ExperimentalTriggeringUpdatesHandlerRemote, WebClientInitialState} from '../../glic.mojom-webui.js';
import {ObservableValue} from '../../observable.js';
import type {ObservableValueReadOnly} from '../../observable.js';
import {TaskQueue} from '../../task_queue.js';
import {ActorClientImpl, ActorHostMessageHandler} from '../actor/actor_host.js';
import {ActorClientDef, ActorHostDef} from '../actor/actor_types.js';
import {AnnotationHostMessageHandler} from '../annotation/annotation_host.js';
import {AnnotationHostDef} from '../annotation/annotation_types.js';
import type {AnnotationHost} from '../annotation/annotation_types.js';
import {ExperimentalTriggeringClientImpl} from '../experimental_triggering/experimental_triggering_host.js';
import {ExperimentalTriggeringClientDef} from '../experimental_triggering/experimental_triggering_types.js';
import type {ExperimentalTriggeringClient} from '../experimental_triggering/experimental_triggering_types.js';
import {maybeWrapWithLogging} from '../mojo_logging.js';
import {getHostRequestHistogramInfo} from '../request_types.js';
import type {ActorClient, ActorHost, WebClient, ZeroStateSuggestionsHost} from '../request_types.js';
import type {ResponseExtras} from '../transport/messaging.js';
import type {InterfaceDef, PendingReceiver, PendingRemote, PostMessageLifecycleObserver, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';
import {ZeroStateSuggestionsHostMessageHandler} from '../zero_state_suggestions/zero_state_suggestions_host.js';
import {ZeroStateSuggestionsHostDef} from '../zero_state_suggestions/zero_state_suggestions_types.js';

import {urlFromClient} from './conversions.js';
import {HostMessageHandler} from './host_from_client.js';
import type {CaptureRegionObserverImpl, PinCandidatesObserverImpl} from './host_from_client.js';
import {PanelOpenState} from './types.js';


export enum WebClientState {
  UNINITIALIZED,
  WARMED,
  RESPONSIVE,
  UNRESPONSIVE,
  ERROR,  // Final state
}


// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum DetailedWebClientState {
  BOOTSTRAP_PENDING = 0,
  WEB_CLIENT_NOT_CREATED = 1,
  WEB_CLIENT_INITIALIZE_FAILED = 2,
  WEB_CLIENT_NOT_INITIALIZED = 3,
  // OBSOLETE: TEMPORARY_UNRESPONSIVE = 4,
  // OBSOLETE: PERMANENT_UNRESPONSIVE = 5,
  RESPONSIVE = 6,
  // OBSOLETE: RESPONSIVE_INACTIVE = 7,
  // OBSOLETE: UNRESPONSIVE_INACTIVE = 8,
  // OBSOLETE: MOJO_PIPE_CLOSED_UNEXPECTEDLY = 9,
  MOJO_PIPE_CLOSED_UNEXPECTEDLY_BEFORE_INITIALIZE = 10,
  MOJO_PIPE_CLOSED_UNEXPECTEDLY_AFTER_INITIALIZE = 11,
  MAX_VALUE = MOJO_PIPE_CLOSED_UNEXPECTEDLY_AFTER_INITIALIZE,
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
  sender: PostMessageRemote<WebClient>;
  panelIsActive = false;

  readonly handler: WebClientHandlerRemote;
  get handlerForTesting(): WebClientHandlerRemote {
    return this.handler;
  }
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
  detailedWebClientState = DetailedWebClientState.BOOTSTRAP_PENDING;
  // Present while the client is monitoring pin candidates.
  pinCandidatesObserver?: PinCandidatesObserverImpl;
  captureRegionObserver?: CaptureRegionObserverImpl;

  actorHandler?: ActorHandlerRemote;
  annotationHandler?: AnnotationHandlerRemote;
  readonly router: PostMessageRouter;

  zeroStateSuggestionsHandler?: ZeroStateSuggestionsHandlerRemote;
  private isDestroyed = false;
  private isSubscribedToZoomLevel = false;
  private zoomFactor?: number;

  private experimentalTriggeringUpdatesHandler =
      new Map<number, ExperimentalTriggeringUpdatesHandlerRemote>();
  private nextExperimentalTriggeringUpdateHandlerId = 0;

  constructor(
      hostRemote: PostMessageRemote<WebClient>,
      hostRouter: PostMessageRouter,
  ) {
    this.router = hostRouter;
    this.sender = hostRemote;
    this.handler = maybeWrapWithLogging(
        new WebClientHandlerRemote(), {prefix: 'WebClientHandler'});
    this.handler.onConnectionError.addListener(() => {
      if (this.isDestroyed ||
          this.webClientState.getCurrentValue() === WebClientState.ERROR) {
        return;
      }
      console.warn(`Mojo connection error in glic host`);
      this.detailedWebClientState = this.detailedWebClientState ===
              DetailedWebClientState.BOOTSTRAP_PENDING ?
          DetailedWebClientState
              .MOJO_PIPE_CLOSED_UNEXPECTEDLY_BEFORE_INITIALIZE :
          DetailedWebClientState.MOJO_PIPE_CLOSED_UNEXPECTEDLY_AFTER_INITIALIZE;
      this.webClientState.assignAndSignal(WebClientState.ERROR);
    });
    const receiver = this.handler.$.bindNewPipeAndPassReceiver();
    receiver.bindInBrowser();

    this.hostMessageHandler = new HostMessageHandler(this.handler, this);
    if (this.router.receiver) {
      this.router.receiver.requestObserver = this;
      this.router.receiver.setHandlerWrapper(this.handlerWrapper.bind(this));
    }
  }

  destroy() {
    this.isDestroyed = true;
    this.webClientState = ObservableValue.withValue<WebClientState>(
        WebClientState.ERROR);  // Final state
    this.hostMessageHandler.destroy();
    this.pinCandidatesObserver?.disconnectFromSource();
    this.captureRegionObserver?.destroy();
    if (this.actorHandler) {
      this.actorHandler.$.close();
      this.actorHandler = undefined;
    }
    if (this.annotationHandler) {
      this.annotationHandler.$.close();
      this.annotationHandler = undefined;
    }
    for (const handler of this.experimentalTriggeringUpdatesHandler.values()) {
      handler.$.close();
    }
    this.experimentalTriggeringUpdatesHandler.clear();
  }

  setInitialState(initialState: WebClientInitialState): {
    actorRemote?: PendingRemote<ActorHost>,
    actorReceiver?: PendingReceiver<ActorClient>,
    experimentalTriggeringReceiver?: PendingReceiver<
                                      ExperimentalTriggeringClient>,
    zeroStateSuggestionsRemote?: PendingRemote<ZeroStateSuggestionsHost>,
  } {
    this.panelIsActive = initialState.panelIsActive;
    this.zoomFactor = initialState.zoomFactor;

    let actorRemote: PendingRemote<ActorHost>|undefined;
    let actorReceiver: PendingReceiver<ActorClient>|undefined;

    if (initialState.enableActInFocusedTab) {
      this.actorHandler = maybeWrapWithLogging(
          new ActorHandlerRemote(), {prefix: 'ActorHandler'});
      const {remote: clientRemote, receiver: receiverVal} =
          this.router.newPipeWithRemote(ActorClientDef);
      const actorClientReceiver =
          new ActorClientReceiver(new ActorClientImpl(clientRemote));
      this.handler.createActorHandler(
          this.actorHandler.$.bindNewPipeAndPassReceiver(),
          actorClientReceiver.$.bindNewPipeAndPassRemote());
      const actorHostMessageHandler =
          new ActorHostMessageHandler(this.actorHandler);
      const {remote: hostRemote} = this.router.newPipeWithReceiver(
          actorHostMessageHandler, ActorHostDef);
      actorRemote = hostRemote;
      actorReceiver = receiverVal;
    }

    const {remote: clientRemote, receiver: experimentalTriggeringReceiver} =
        this.router.newPipeWithRemote(ExperimentalTriggeringClientDef);
    const experimentalTriggeringClientReceiver =
        new ExperimentalTriggeringClientReceiver(
            new ExperimentalTriggeringClientImpl(clientRemote, this));
    this.handler.createExperimentalTriggeringClient(
        experimentalTriggeringClientReceiver.$.bindNewPipeAndPassRemote());

    let zeroStateSuggestionsRemote: PendingRemote<ZeroStateSuggestionsHost>|
        undefined;
    if (initialState.enableZeroStateSuggestions) {
      this.zeroStateSuggestionsHandler = maybeWrapWithLogging(
          new ZeroStateSuggestionsHandlerRemote(),
          {prefix: 'ZeroStateSuggestionsHandler'});
      this.handler.createZeroStateSuggestionsHandler(
          this.zeroStateSuggestionsHandler.$.bindNewPipeAndPassReceiver());
      const zeroStateSuggestionsHostMessageHandler =
          new ZeroStateSuggestionsHostMessageHandler(
              this.zeroStateSuggestionsHandler, this.router);
      const {remote: zeroStateSuggestionsRemoteVal} =
          this.router.newPipeWithReceiver(
              zeroStateSuggestionsHostMessageHandler,
              ZeroStateSuggestionsHostDef);
      zeroStateSuggestionsRemote = zeroStateSuggestionsRemoteVal;
    }

    return {
      actorRemote,
      actorReceiver,
      experimentalTriggeringReceiver,
      zeroStateSuggestionsRemote,
    };
  }

  createAnnotationHandler(receiver: PendingReceiver<AnnotationHost>): void {
    assert(!this.annotationHandler);
    this.annotationHandler = maybeWrapWithLogging(
        new AnnotationHandlerRemote(), {prefix: 'AnnotationHandler'});
    this.handler.createAnnotationHandler(
        this.annotationHandler.$.bindNewPipeAndPassReceiver());
    const annotationHostMessageHandler =
        new AnnotationHostMessageHandler(this.annotationHandler);
    this.router.newReceiver(
        receiver, annotationHostMessageHandler, AnnotationHostDef);
  }

  subscribeToZoomLevel() {
    this.isSubscribedToZoomLevel = true;
    if (this.zoomFactor !== undefined) {
      this.sender.requestNoResponse(
          'notifyZoomLevelChanged', {zoomFactor: this.zoomFactor});
    }
  }

  unsubscribeFromZoomLevel() {
    this.isSubscribedToZoomLevel = false;
  }

  onZoomLevelChanged(zoomFactor: number) {
    this.zoomFactor = zoomFactor;
    if (this.isSubscribedToZoomLevel) {
      this.sender.requestNoResponse('notifyZoomLevelChanged', {zoomFactor});
    }
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

  getInstanceIsActive(): boolean {
    return this.instanceIsActive;
  }

  isPanelOpen(): boolean {
    return this.panelOpenState === PanelOpenState.OPEN;
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

  openLinkInPopup(url: string, initialWidth: number, initialHeight: number) {
    this.handler.openLinkInPopup(
        urlFromClient(url), initialWidth, initialHeight);
  }

  async openLinkInNewTab(url: string) {
    await this.handler.createTab(urlFromClient(url), {
      openInBackground: false,
      windowId: null,
    });
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

    const startTime = performance.now();
    const response = await handlerFunction(payload, extras);
    if (response) {
      // Report latency metric for handled requests that return a response.
      const latency = performance.now() - startTime;
      this.reportLatency(type, interfaceDef, latency);
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
    let mojoEvent = MojomGlicRequestEvent.kRequestReceived;
    switch (event) {
      case GlicRequestEvent.REQUEST_RECEIVED:
        mojoEvent = MojomGlicRequestEvent.kRequestReceived;
        break;
      case GlicRequestEvent.RESPONSE_SENT:
        mojoEvent = MojomGlicRequestEvent.kResponseSent;
        break;
      case GlicRequestEvent.REQUEST_HANDLER_EXCEPTION:
        mojoEvent = MojomGlicRequestEvent.kRequestHandlerException;
        break;
      case GlicRequestEvent.REQUEST_RECEIVED_WHILE_INACTIVE:
        mojoEvent = MojomGlicRequestEvent.kRequestReceivedWhileInactive;
        break;
      default:
        return;
    }
    try {
      this.handler.reportApiRequestCount(histogramInfo.id, mojoEvent);
    } catch (e) {
      console.error('[reportApiRequestCount ERROR]', e);
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
    try {
      this.handler.reportApiRequestLatency(
          histogramInfo.name,
          {microseconds: BigInt(Math.round(latencyMs * 1000))});
    } catch (e) {
      console.error('[reportApiRequestLatency ERROR]', e);
    }
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
