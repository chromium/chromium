// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {InterfaceDef, InterfaceDefMethods, RequestPayload, ResponsePayload} from './messaging.js';
import {ResponseExtras} from './messaging.js';

export type {
  InterfaceDef,
  InterfaceDefMethods,
  RequestPayload,
  ResponsePayload,
};

// This file contains helpers to send and receive messages over postMessage.

// The receiving end of a pipe which is bound to a message handler.
export interface PostMessageReceiver {
  // Close the pipe. Should be called when no longer needed.
  close(): void;
  // Set the message handler for this pipe. Note that this is only needed if the
  // handler needs to change after creating the receiver with
  // `PostMessageRouter.newReceiver`. Triggers pending messages to be
  // dispatched to the handler in a posted task.
  setMessageHandler<I extends InterfaceDef>(
      handler: PostMessageHandler<I>, interfaceDef?: I): void;
  // Add a callback to be called when the pipe is closed. Calls `f` immediately
  // if the pipe is already closed.
  addCloseHandler(f: () => void): void;
}

// The sending end of a pipe.
export interface PostMessageRemote<I extends InterfaceDef> {
  // Close the pipe. Should be called when no longer needed.
  close(): void;
  // Add a callback to be called when the pipe is closed. Calls `f` immediately
  // if the pipe is already closed.
  addCloseHandler(f: () => void): void;
  // Send a request, resolves with the received response.
  // If the pipe is closed before a response is received, the promise rejects.
  // If the message is sent before the pipe is bound to a receiver, the
  // message will queued until a receiver is bound.
  requestWithResponse<T extends keyof InterfaceDefMethods<I>>(
      requestType: T, request: RequestPayload<InterfaceDefMethods<I>, T>,
      transfer?: Transferable[]):
      Promise<ResponsePayload<InterfaceDefMethods<I>, T>>;
  // Send a request, does not wait for a response.
  // If the message is sent before the pipe is bound to a receiver, the
  // message will queued until a receiver is bound.
  requestNoResponse<T extends keyof InterfaceDefMethods<I>>(
      requestType: T, request: RequestPayload<InterfaceDefMethods<I>, T>,
      transfer?: Transferable[]): void;
  // The raw sender for this pipe.
  rawSender(): PostMessageRequestSender;
}

// An exception which can be sent over postMessage.
export interface TransferableException {
  exception: Error;
  [key: string]: unknown;
}

// The interface for encoding and decoding errors for transfer across
// postMessage.
export interface ErrorCodec {
  serialize(e: Error): TransferableException;
  deserialize(raw: TransferableException): Error;
}

declare const brand: unique symbol;
// Identifies the receiving end of a pipe which has not yet been bound.
// Can be sent over postMessage.
// Can be used to create a PostMessageReceiver.
export type PendingReceiver<I extends InterfaceDef> =
    number&{[brand]: {receiver: I}};

// Identifies the sending end of a pipe which has not yet been bound.
// Can be sent over postMessage.
// Can be used to create a PostMessageRemote.
export type PendingRemote<I extends InterfaceDef> =
    number&{[brand]: {remote: I}};

// Define the root pipe. It's just the number 0, but we use a branded type to
// give it a distinct type.
declare const rootPipeBrand: unique symbol;
type RootPipe = number&{[rootPipeBrand]: true};
const ROOT_PIPE = 0 as RootPipe;

// Manages pipes and provides a way to send and receive messages over
// `postMessage`.
export interface PostMessageRouter {
  // Creates a new pipe bound to a pending receiver and a remote.
  newPipeWithRemote<RemoteInterface extends InterfaceDef>(
      remoteInterfaceDef: RemoteInterface): {
    receiver: PendingReceiver<RemoteInterface>,
    remote: PostMessageRemote<RemoteInterface>,
  };

  // Creates a new pipe bound to a receiver and a pending remote.
  newPipeWithReceiver<ReceiverInterface extends InterfaceDef>(
      receiverHandler: PostMessageHandler<ReceiverInterface>,
      interfaceDef: ReceiverInterface): {
    receiver: PostMessageReceiver,
    remote: PendingRemote<ReceiverInterface>,
  };

  // Create a PostMessageRemote from a PendingRemote.
  newRemote<I extends InterfaceDef>(remote: PendingRemote<I>|
                                    RootPipe): PostMessageRemote<I>;

  // Create a PostMessageReceiver from a PendingReceiver.
  newReceiver<I extends InterfaceDef>(
      receiver: PendingReceiver<I>, handler: PostMessageHandler<I>,
      interfaceDef: I): PostMessageReceiver;

  // Destroy all pipes created by this router.
  destroy(): void;

  // Enable logging for all messages.
  setLoggingEnabled(enabled: boolean): void;
}

export const ON_PIPE_CLOSED = Symbol('ON_PIPE_CLOSED');

declare interface MessageBase {
  // In RequestMessage, this is the Unique ID of the sender in RequestMessage.
  // In ResponseMessage, this is the round-tripped senderId (id of the other
  // side).
  senderId: string;
  // The type of request.
  type: string;
  // The id of the pipe this request belongs to.
  pipeId: number;
}

// Requests sent over postMessage have this structure.
export declare interface RequestMessage extends MessageBase {
  // Present for any Glic request message.
  glicRequest: true;
  // A unique ID of the request. Round-tripped in the response. `undefined` if a
  // response is not desired.
  requestId?: number;
  // A payload. Each type of request has a distinct payload type.
  requestPayload: unknown;
}

// Responses sent over postMessage have this structure. Responses are messages
// sent in response to a `RequestMessage`.
declare interface ResponseMessage extends MessageBase {
  // The round-tripped `RequestMessage.requestId`.
  responseId: number;
  // A payload. Each type of response has a distinct payload type. Not set if
  // exception is set.
  responsePayload?: unknown;
  // An error that occurred during processing the request. If this is set,
  // responsePayload will not be set.
  exception?: TransferableException;
}

// Something that has postMessage() - probably a window or WindowProxy.
export declare interface PostMessageSender {
  postMessage(
      message: unknown, targetOrigin: string, transfer?: Transferable[]): void;
}

function newSenderId(): string {
  const array = new Uint8Array(8);
  crypto.getRandomValues(array);
  return Array.from(array).map((n: number) => n.toString(16)).join('');
}

class MessageLogger {
  loggingEnabled = false;
  loggingPrefix: string;
  constructor(senderId: string, protected prefix: string) {
    this.loggingPrefix = `${prefix}(${senderId.substring(0, 6)})`;
  }

  setLoggingEnabled(v: boolean): void {
    this.loggingEnabled = v;
  }

  shouldLogMessage(requestType: string): boolean {
    return this.loggingEnabled &&
        requestType !== 'glicWebClientCheckResponsive';
  }

  maybeLogMessage(requestType: string, message: string, payload: unknown) {
    if (!this.shouldLogMessage(requestType)) {
      return;
    }
    console.info(
        `${this.loggingPrefix} [${requestType}] ${message}: ${
            toDebugJson(payload)}`,
        payload);
  }
}

// Implements a simple queue with O(1) push and shift.
export class Queue<T> {
  // An array where elements are always pushed.
  next: T[] = [];
  // An array frozen in size from which elements are consumed.
  // When all elements are consumed, `next` is swapped into `current`.
  current: Array<T|undefined> = [];
  // Next element index in `current`.
  index = 0;

  push(item: T): void {
    this.next.push(item);
  }

  popFront(): T|undefined {
    if (this.index < this.current.length) {
      const result = this.current[this.index]!;
      this.current[this.index] = undefined;
      this.index++;
      return result;
    }
    if (this.next.length === 0) {
      return undefined;
    }
    this.current = this.next;
    this.next = [];
    this.index = 0;
    return this.popFront();
  }

  get length(): number {
    return this.current.length - this.index + this.next.length;
  }

  empty(): boolean {
    return this.next.length === 0 && this.index === this.current.length;
  }
}

interface PipeInterface {
  readonly open: boolean;
  readonly messageHandler: unknown;
  interfaceDef?: InterfaceDef;
  addCloseHandler(f: () => void): void;
  close(): void;
  setMessageHandler(handler: unknown, interfaceDef?: InterfaceDef): void;
  pending(): boolean;
  pushPendingMessage(
      pmReceiver: PostMessageRequestReceiver,
      requestMessage: RequestMessage): void;
  getHandlerFunction(type: string): HandlerFunction|undefined;
}

class ClosedPipe implements PipeInterface {
  open = false;
  readonly messageHandler = undefined;
  interfaceDef = undefined;
  addCloseHandler(f: () => void): void {
    f();
  }
  close(): void {}
  setMessageHandler(_handler: unknown, _interfaceDef?: InterfaceDef): void {}
  getHandlerFunction(_type: string): HandlerFunction|undefined {
    return undefined;
  }
  pending(): boolean {
    return false;
  }
  pushPendingMessage(
      _pmReceiver: PostMessageRequestReceiver,
      _requestMessage: RequestMessage): void {}
}

const CLOSED_PIPE = new ClosedPipe();

// Used by PostMessageRouterImpl to track a pipe.
class Pipe implements PipeInterface {
  private onCloseHandlers: Array<() => void> = [];
  open = true;
  // The message handler for this pipe, if the pipe is bound as a receiver.
  messageHandler: unknown;
  interfaceDef?: InterfaceDef;
  private pendingMessages: Array<[PostMessageRequestReceiver, RequestMessage]> =
      [];
  constructor(
      // The id of the pipe, which is a unique number.
      public readonly id: number) {}

  close() {
    if (!this.open) {
      return;
    }
    this.open = false;
    this.onCloseHandlers.forEach((f: () => void) => {
      try {
        f();
      } catch (e) {
        console.error('Error in pipe onClose callback', e);
      }
    });
    this.onCloseHandlers = [];
    this.pendingMessages = [];
  }

  addCloseHandler(f: () => void): void {
    if (this.open) {
      this.onCloseHandlers.push(f);
    } else {
      f();
    }
  }

  setMessageHandler<I extends InterfaceDef>(handler: unknown, interfaceDef: I):
      void {
    this.messageHandler = handler;
    this.interfaceDef = interfaceDef;
    if (this.messageHandler && this.pendingMessages.length) {
      const messages = this.pendingMessages;
      this.pendingMessages = [];
      if (!this.open) {
        return;
      }
      setTimeout(() => {
        messages.forEach(([pmReceiver, requestMessage]) => {
          pmReceiver.onMessage(requestMessage);
        });
      }, 0);
    }
  }

  getHandlerFunction(type: string): HandlerFunction|undefined {
    if (!this.open) {
      return undefined;
    }
    if (this.messageHandler) {
      const handleFn = (this.messageHandler as Record<string, unknown>)[type];
      if (typeof handleFn !== 'function') {
        return undefined;
      }
      return handleFn.bind(this.messageHandler) as HandlerFunction;
    }
    return undefined;
  }

  pending(): boolean {
    return this.open &&
        (!this.messageHandler || this.pendingMessages.length > 0);
  }

  pushPendingMessage(
      pmReceiver: PostMessageRequestReceiver,
      requestMessage: RequestMessage): void {
    this.pendingMessages.push([pmReceiver, requestMessage]);
  }
}

export class PostMessageRouterImpl extends MessageLogger implements
    PostMessageRouter {
  private onDestroy: () => void;
  sender?: PostMessageRequestSender;
  receiver?: PostMessageRequestReceiver;
  // Pipes that are not closed.
  readonly pipes: Map<number, PipeInterface> = new Map();
  // Tracks IDs of pipes that have been closed.
  readonly closedPipes = new InverseSet();
  private nextPipeId: number;

  constructor(
      public readonly remoteOrigin: string, readonly senderId: string,
      readonly messageSender: PostMessageSender, logPrefix: string,
      public readonly isHost: boolean,
      public readonly errorCodec: ErrorCodec = {
        serialize: e => ({exception: e}),
        deserialize: raw => raw.exception,
      }) {
    super(senderId, logPrefix);
    // Use even and odd scheme to ensure host and client pipe ids don't clash.
    this.nextPipeId = isHost ? 2 : 1;
    const handler = this.onMessage.bind(this);
    window.addEventListener('message', handler);
    this.onDestroy = () => {
      window.removeEventListener('message', handler);
    };
  }

  destroy() {
    this.onDestroy();
  }

  onMessage(event: MessageEvent) {
    if (event.origin !== this.remoteOrigin) {
      return;
    }
    // Check properties on MessageBase.
    const data = event.data;
    if (data.type === undefined || data.senderId === undefined ||
        !event.source) {
      return;
    }

    if (data.responseId === undefined) {
      // For RequestMessage, only process messages if they are not from this
      // sender.
      if (this.receiver && data.senderId !== this.senderId &&
          data.glicRequest === true) {
        this.receiver?.onMessage(data);
      }
    } else {
      // For ResponseMessage, only process messages if they are from this
      // sender.
      if (this.sender && data.senderId === this.senderId) {
        this.sender?.onMessage(data);
      }
    }
  }

  sendRequest(
      pipeId: number, type: string, requestId: number|undefined,
      requestPayload: unknown, transfer: Transferable[] = []) {
    const request = {
      glicRequest: true,
      pipeId,
      type,
      requestId,
      requestPayload,
      senderId: this.senderId,
    } satisfies RequestMessage;
    this.maybeLogMessage(type, 'sending request', request);
    this.messageSender.postMessage(request, this.remoteOrigin, transfer);
  }

  sendResponse(
      pipeId: number, type: string, senderId: string, responseId: number,
      responsePayload: unknown, exception: TransferableException|undefined,
      transfer: Transferable[] = []) {
    const response: ResponseMessage = {
      pipeId,
      type,
      responseId,
      responsePayload,
      senderId,
    };
    if (exception) {
      response.exception = exception;
    }
    this.maybeLogMessage(type, 'sending response', response);
    this.messageSender.postMessage(response, this.remoteOrigin, transfer);
  }

  newPipe<I extends InterfaceDef>(): {
    receiver: PendingReceiver<I>,
    remote: PendingRemote<I>,
  } {
    const id = this.nextPipeId;
    this.nextPipeId += 2;
    return {
      receiver: id as unknown as PendingReceiver<I>,
      remote: id as unknown as PendingRemote<I>,
    };
  }

  newPipeWithReceiver<ReceiverInterface extends InterfaceDef>(
      receiverHandler: PostMessageHandler<ReceiverInterface>,
      interfaceDef: ReceiverInterface): {
    receiver: PostMessageReceiver,
    remote: PendingRemote<ReceiverInterface>,
  } {
    const {receiver, remote} = this.newPipe<ReceiverInterface>();
    return {
      receiver: this.newReceiver(receiver, receiverHandler, interfaceDef),
      remote,
    };
  }

  newPipeWithRemote<RemoteInterface extends InterfaceDef>(
      _remoteInterfaceDef: RemoteInterface): {
    receiver: PendingReceiver<RemoteInterface>,
    remote: PostMessageRemote<RemoteInterface>,
  } {
    const {receiver, remote} = this.newPipe<RemoteInterface>();
    return {
      receiver,
      remote: this.newRemote<RemoteInterface>(remote),
    };
  }

  newReceiver<I extends InterfaceDef>(
      receiver: PendingReceiver<I>|RootPipe, handler: PostMessageHandler<I>,
      interfaceDef: I): PostMessageReceiver {
    const pipe = this.getOrMakePipe(receiver as number);
    if (pipe.open && handler) {
      pipe.setMessageHandler(handler, interfaceDef);
    }
    return new PostMessageReceiverImpl(pipe, receiver as number, handler, this);
  }

  newRemote<I extends InterfaceDef>(remote: PendingRemote<I>|
                                    RootPipe): PostMessageRemote<I> {
    this.getOrMakePipe(remote as number);
    return new PostMessageRemoteImpl<I>(remote, this.sender!, this);
  }

  closePipe(
      pipe: PendingReceiver<InterfaceDef>|PendingRemote<InterfaceDef>|number) {
    this.closedPipes.add(pipe);
    const existing = this.pipes.get(pipe);
    if (!existing) {
      return;
    }
    this.pipes.delete(pipe);
    existing.close();
    this.sendRequest(pipe, '__closePipe', undefined, undefined);
  }

  remoteClosedPipe(pipeId: number) {
    this.closedPipes.add(pipeId);
    const pipe = this.pipes.get(pipeId);
    if (pipe) {
      pipe.close();
      this.pipes.delete(pipeId);
    }
  }

  getOrMakePipe(id: number): PipeInterface {
    if (this.closedPipes.has(id)) {
      return CLOSED_PIPE;
    }
    let pipe = this.pipes.get(id);
    if (!pipe) {
      pipe = new Pipe(id);
      this.pipes.set(id, pipe);
    }
    return pipe;
  }
}



export type PostMessageHandler<I extends InterfaceDef> = {
  [Property in keyof InterfaceDefMethods<I>]: (
      payload: RequestPayload<InterfaceDefMethods<I>, Property>,
      extras: ResponseExtras,
      ) => Promise<ResponsePayload<InterfaceDefMethods<I>, Property>>|
  ResponsePayload<InterfaceDefMethods<I>, Property>;
}&{
  // Handlers may also implement a function with this key, and it will be called
  // when the pipe is closed.
  [ON_PIPE_CLOSED]?: () => void,
};

export class PostMessageRemoteImpl<I extends InterfaceDef> implements
    PostMessageRemote<I> {
  constructor(
      public readonly pipeId: PendingRemote<I>|RootPipe,
      public sender: PostMessageRequestSender,
      private router: PostMessageRouterImpl) {}

  addCloseHandler(f: () => void) {
    const pipe = this.router.pipes.get(this.pipeId as number);
    pipe?.addCloseHandler(f);
  }

  close() {
    this.router.closePipe(this.pipeId as number);
  }

  requestWithResponse<T extends keyof InterfaceDefMethods<I>>(
      requestType: T, request: RequestPayload<InterfaceDefMethods<I>, T>,
      transfer: Transferable[] = []):
      Promise<ResponsePayload<InterfaceDefMethods<I>, T>> {
    return this.sender.requestWithResponse(
               this.pipeId as number, requestType as string, request,
               transfer) as Promise<ResponsePayload<InterfaceDefMethods<I>, T>>;
  }

  requestNoResponse<T extends keyof InterfaceDefMethods<I>>(
      requestType: T, request: RequestPayload<InterfaceDefMethods<I>, T>,
      transfer: Transferable[] = []): void {
    this.sender.requestNoResponse(
        this.pipeId as number, requestType as string, request, transfer);
  }

  rawSender(): PostMessageRequestSender {
    return this.sender;
  }
}

/**
 * Receives messages sent via a `PostMessageRemote`, and forwards them to a
 * message handler.
 */
export class PostMessageReceiverImpl implements PostMessageReceiver {
  constructor(
      public readonly pipe: PipeInterface, public readonly pipeId: number,
      public handler: PostMessageHandler<InterfaceDef>|undefined,
      private router: PostMessageRouterImpl) {
    this.pipe.addCloseHandler(() => {
      this.handler?.[ON_PIPE_CLOSED]?.();
    });
  }

  close() {
    this.router.closePipe(this.pipeId);
  }

  setMessageHandler<I extends InterfaceDef>(
      handler: PostMessageHandler<I>, interfaceDef: I): void {
    this.pipe.setMessageHandler(handler, interfaceDef);
  }

  addCloseHandler(f: () => void) {
    this.pipe.addCloseHandler(f);
  }
}

// Sends requests over postMessage.
export class PostMessageRequestSender {
  requestId = 1;
  responseHandlers:
      Map<number,
          {type: string, handler: (response: ResponseMessage) => void}> =
          new Map();

  // We limit the number of in-flight requests because it fails in a better way
  // than doing no limiting. The WebUI code heavily relies on promises to
  // dispatch work in processing mojo requests and requests over postMessage.
  // Ideally, we could prioritize processing of mojo requests, but that's not
  // currently tractable. Instead, we limit the number of in-flight requests
  // which in practice prevents the situation where the WebUI is too busy
  // processing new postMessage requests to respond to existing ones.

  private maxInFlightRequests = Infinity;
  // If true, send responses for all requests even if the request doesn't
  // require one. This ensures in-flight request tracking is accurate.
  // Without this, in-flight request tracking just ignores requests that don't
  // require a response until after the `maxInFlightRequests` limit is reached.
  sendResponsesForAllRequests = false;
  sendQueue = new Queue<() => void>();
  queueNoticeLogged = false;

  constructor(private router: PostMessageRouterImpl) {
    assert(router.sender === undefined);
    router.sender = this;
  }

  inFlightRequestCount(): number {
    return this.responseHandlers.size;
  }

  messageQueueLength(): number {
    return this.sendQueue.length;
  }

  // Handles responses from the host.
  onMessage(response: ResponseMessage) {
    const entry = this.responseHandlers.get(response.responseId);
    if (!entry) {
      // No handler for this request.
      return;
    }
    this.responseHandlers.delete(response.responseId);
    if (!this.sendQueue.empty() &&
        this.responseHandlers.size < this.maxInFlightRequests) {
      // Running a queued send will always register another response handler.
      // This should ensure a steady state of MAX_IN_FLIGHT_REQUESTS until
      // the queue is empty.
      this.sendQueue.popFront()!();
    }
    entry.handler(response);
  }

  // Sends a request to the host, and returns a promise that resolves with its
  // response.
  requestWithResponse(
      pipeId: number, requestType: string, request: unknown,
      transfer: Transferable[] = []): Promise<unknown> {
    const {promise, resolve, reject} = Promise.withResolvers<unknown>();
    const requestId = this.requestId++;
    const processFn = () => {
      this.responseHandlers.set(requestId, {
        type: requestType,
        handler: (response: ResponseMessage) => {
          if (response.exception !== undefined) {
            this.router.maybeLogMessage(
                requestType, 'received response with exception',
                response.exception);
            reject(this.router.errorCodec.deserialize(response.exception));
          } else {
            this.router.maybeLogMessage(
                requestType, 'received response', response.responsePayload);
            resolve(response.responsePayload);
          }
        },
      });

      this.router.sendRequest(
          pipeId, requestType, requestId, request, transfer);
    };
    if (this.isQueueing()) {
      if (!this.queueNoticeLogged) {
        console.warn(
            `WARNING! ${this.router.loggingPrefix}:` +
            ` Too many in-flight requests, starting to queue them.`);
        this.logInFlightRequestsForDebugging();
        this.queueNoticeLogged = true;
      }
      this.sendQueue.push(processFn);
    } else {
      processFn();
    }

    return promise;
  }

  requestNoResponse(
      pipeId: number, requestType: string, request: unknown,
      transfer: Transferable[] = []): void {
    if (!this.sendResponsesForAllRequests && !this.isQueueing()) {
      this.router.sendRequest(
          pipeId, requestType, undefined, request, transfer);
      return;
    }
    this.requestWithResponse(pipeId, requestType, request, transfer);
  }

  setMaxInFlightRequests(maxInFlightRequests: number) {
    if (maxInFlightRequests < 1) {
      this.maxInFlightRequests = Infinity;
    } else {
      this.maxInFlightRequests = maxInFlightRequests;
    }
  }

  private isQueueing() {
    return this.inFlightRequestCount() >= this.maxInFlightRequests ||
        this.sendQueue.length > 0;
  }

  private logInFlightRequestsForDebugging() {
    const counts: Map<string, number> = new Map();
    for (const entry of this.responseHandlers.values()) {
      counts.set(entry.type, (counts.get(entry.type) || 0) + 1);
    }
    const entries = Array.from(counts.entries());
    entries.sort((a, b) => b[1] - a[1]);
    const entriesString =
        entries.map(([type, count]) => `${type}: ${count}`).join(', ');
    console.info(
        `${this.router.loggingPrefix}: In-flight requests: ${entriesString}`);
  }
}

/** Observer for postMessage request lifecycle events. */
export interface PostMessageLifecycleObserver {
  /**
   * Called when each request is received. Messages received on closed pipes do
   * not trigger this. Messages sent to pending pipes trigger this only after
   * the receiver is created.
   */
  onRequestReceived(type: string, interfaceDef: InterfaceDef|undefined): void;
  /** Called when a request handler throws an exception. */
  onRequestHandlerException(type: string, interfaceDef: InterfaceDef|undefined):
      void;
  /**
   * Called when a request response is sent (will not be called if
   * `onRequestHandlerException()` is called.).
   */
  onRequestCompleted(type: string, interfaceDef: InterfaceDef|undefined): void;
}

type HandlerFunction = (payload: unknown, extras: ResponseExtras) =>
    Promise<unknown>;

type HandlerWrapper =
    (type: string, interfaceDef: InterfaceDef|undefined, payload: unknown,
     extras: ResponseExtras, handler: HandlerFunction) => Promise<unknown>;

// Receives requests over postMessage and forward them to a
// `PostMessageLifecycleObserver`.
export class PostMessageRequestReceiver {
  private handlerWrapper: HandlerWrapper =
      (_type, _interfaceName, payload, extras, handler) =>
          handler(payload, extras);
  constructor(
      private router: PostMessageRouterImpl,
      public requestObserver: PostMessageLifecycleObserver) {
    assert(router.receiver === undefined);
    router.receiver = this;
  }

  setHandlerWrapper(wrapper: HandlerWrapper) {
    this.handlerWrapper = wrapper;
  }

  async onMessage(requestMessage: RequestMessage) {
    const {pipeId, senderId, requestId, type, requestPayload} = requestMessage;

    if (type === '__closePipe') {
      this.router.remoteClosedPipe(requestMessage.pipeId);
      return;
    }

    const pipe = this.router.getOrMakePipe(pipeId);
    // TODO(harringtond): Consider adding more debug logging for pending
    // messages or messages dropped due to pipe closure.
    if (pipe.pending()) {
      pipe.pushPendingMessage(this, requestMessage);
      return;
    }

    if (!pipe.open) {
      const exception =
          this.router.errorCodec.serialize(new Error(`Pipe closed`));

      if (requestId) {
        this.router.sendResponse(
            pipeId, type, senderId, requestId, undefined, exception);
      }
      return;
    }
    this.requestObserver.onRequestReceived(type, pipe.interfaceDef);

    const handleFn = pipe.getHandlerFunction(type);
    if (!handleFn) {
      return;
    }
    this.router.maybeLogMessage(type, 'processing request', requestPayload);

    let response;
    let exception: TransferableException|undefined;
    const extras = new ResponseExtras();
    try {
      response = await this.handlerWrapper(
          type, pipe.interfaceDef, requestPayload, extras, handleFn);
    } catch (error) {
      this.requestObserver.onRequestHandlerException(type, pipe.interfaceDef);
      console.warn('Unexpected error', error);
      if (error instanceof Error) {
        exception = this.router.errorCodec.serialize(error);
      } else {
        exception = this.router.errorCodec.serialize(
            new Error(`Unexpected error: ${error}`));
      }
    }

    if (!exception) {
      this.requestObserver.onRequestCompleted(type, pipe.interfaceDef);
    }

    // If the message contains no `requestId`, a response is not requested.
    if (!requestId) {
      return;
    }
    this.router.sendResponse(
        pipeId, type, senderId, requestId, response, exception,
        extras.transfers);
  }
}

export function createBidirectionalPostMessageTransport<
    RemoteInterface extends InterfaceDef,
                            ReceiverInterface extends InterfaceDef>(
    remoteOrigin: string,
    postMessageSender: PostMessageSender,
    lifecycleObserver: PostMessageLifecycleObserver,
    rootMessageHandler: PostMessageHandler<ReceiverInterface>,
    logPrefix: string,
    isHost: boolean,
    errorCodec: ErrorCodec,
    interfaceDef: ReceiverInterface,
    _remoteInterfaceDef: RemoteInterface,
) {
  const senderId = newSenderId();
  const router = new PostMessageRouterImpl(
      remoteOrigin, senderId, postMessageSender, logPrefix, isHost, errorCodec);
  const sender = new PostMessageRequestSender(router);
  const receiver = new PostMessageRequestReceiver(router, lifecycleObserver);
  const rootReceiver =
      router.newReceiver(ROOT_PIPE, rootMessageHandler, interfaceDef);
  const rootRemote =
      new PostMessageRemoteImpl<RemoteInterface>(ROOT_PIPE, sender, router);
  return {router, sender, receiver, rootRemote, rootReceiver};
}

// Converts a value to JSON for debug logging.
function toDebugJson(v: unknown): string {
  return JSON.stringify(v, (_key, value) => {
    // stringify throws on bigint, so convert it.
    if (typeof value === 'bigint') {
      return value.toString();
    }
    if (value instanceof ArrayBuffer) {
      return `ArrayBuffer(${value.byteLength})`;
    }
    if (ArrayBuffer.isView(value)) {
      return `${value.constructor.name}(${value.byteLength})`;
    }
    return value;
  });
}

// Stores a set of non-negative integers in O(N) memory where N is the largest
// integer _not_ stored between [0, M], and M is the largest
// integer stored.
export class InverseSet {
  private notContained: Set<number> = new Set();
  private maxValue = -1;

  add(v: number): void {
    assert(v >= 0);
    while (v > this.maxValue) {
      this.maxValue++;
      this.notContained.add(this.maxValue);
    }
    this.notContained.delete(v);
  }
  delete(v: number): void {
    if (v <= this.maxValue) {
      this.notContained.add(v);
    }
  }
  has(v: number): boolean {
    return v <= this.maxValue && !this.notContained.has(v);
  }
}
