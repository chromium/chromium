// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {AllRequestTypes, AllRequestTypesWithoutReturn, RequestRequestType, RequestResponseType, TransferableException} from './request_types.js';
import {exceptionFromTransferable, newTransferableException} from './request_types.js';

// This file contains helpers to send and receive messages over postMessage.

export declare interface MessageBase {
  // In RequestMessage, this is the Unique ID of the sender in RequestMessage.
  // In ResponseMessage, this is the round-tripped senderId (id of the other
  // side).
  senderId: string;
  // The type of request.
  type: string;
}

// Requests sent over postMessage have this structure.
export declare interface RequestMessage extends MessageBase {
  // Present for any Glic request message.
  glicRequest: true;
  // A unique ID of the request. Round-tripped in the response. `undefined` if a
  // response is not desired.
  requestId?: number;
  // A payload. Each type of request has a distinct payload type.
  requestPayload: any;
}

// Responses sent over postMessage have this structure. Responses are messages
// sent in response to a `RequestMessage`.
declare interface ResponseMessage extends MessageBase {
  // The round-tripped `RequestMessage.requestId`.
  responseId: number;
  // A payload. Each type of response has a distinct payload type. Not set if
  // exception is set.
  responsePayload?: any;
  // An error that occurred during processing the request. If this is set,
  // responsePayload will not be set.
  exception?: TransferableException;
}

// Something that has postMessage() - probably a window or WindowProxy.
declare interface PostMessageSender {
  postMessage(message: any, targetOrigin: string, transfer?: Transferable[]):
      void;
}

export function newSenderId(): string {
  const array = new Uint8Array(8);
  crypto.getRandomValues(array);
  return Array.from(array).map((n: number) => n.toString(16)).join('');
}

export class ResponseExtras {
  transfers: Transferable[] = [];

  // Add objects to transfer when sending the response over postMessage.
  addTransfer(...transfers: Transferable[]): void {
    this.transfers.push(...transfers);
  }
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

  maybeLogMessage(requestType: string, message: string, payload: any) {
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

// Shared functionality between the sender and receiver.
export class PostMessageRouter extends MessageLogger {
  private onDestroy: () => void;
  sender?: PostMessageRequestSender;
  receiver?: PostMessageRequestReceiver;

  constructor(
      public readonly remoteOrigin: string, readonly senderId: string,
      readonly messageSender: PostMessageSender, logPrefix: string) {
    super(senderId, logPrefix);
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
      type: string, requestId: number|undefined, requestPayload: any,
      transfer: Transferable[] = []) {
    const request = {
      glicRequest: true,
      type,
      requestId,
      requestPayload,
      senderId: this.senderId,
    } satisfies RequestMessage;
    this.maybeLogMessage(type, 'sending request', request);
    this.messageSender.postMessage(request, this.remoteOrigin, transfer);
  }

  sendResponse(
      type: string, senderId: string, responseId: number, responsePayload: any,
      exception: TransferableException|undefined,
      transfer: Transferable[] = []) {
    const response: ResponseMessage = {
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
}

// Sends requests over postMessage. Ideally this type would be parameterized by
// only one of HostRequestTypes or WebClientRequestTypes, but typescript
// cannot represent this. Instead, this class can send messages of any type.
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

  constructor(private router: PostMessageRouter) {
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
  requestWithResponse<T extends keyof AllRequestTypes>(
      requestType: T, request: RequestRequestType<T>,
      transfer: Transferable[] = []): Promise<RequestResponseType<T>> {
    const {promise, resolve, reject} =
        Promise.withResolvers<RequestResponseType<T>>();
    const requestId = this.requestId++;
    const processFn = () => {
      this.responseHandlers.set(requestId, {
        type: requestType,
        handler: (response: ResponseMessage) => {
          if (response.exception !== undefined) {
            this.router.maybeLogMessage(
                requestType, 'received response with exception',
                response.exception);
            reject(exceptionFromTransferable(response.exception));
          } else {
            this.router.maybeLogMessage(
                requestType, 'received response', response.responsePayload);
            resolve(response.responsePayload as RequestResponseType<T>);
          }
        },
      });

      this.router.sendRequest(requestType, requestId, request, transfer);
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

  // Sends a request to the host, and does not track the response.
  requestNoResponse<T extends keyof AllRequestTypesWithoutReturn>(
      requestType: T, request: RequestRequestType<T>,
      transfer: Transferable[] = []): void {
    if (!this.sendResponsesForAllRequests && !this.isQueueing()) {
      this.router.sendRequest(requestType, undefined, request, transfer);
      return;
    }
    // When queueing, use requestWithResponse because it supports queueing.
    // This ensures that queued requests are sent steadily as responses are
    // received.
    this.requestWithResponse(requestType, request, transfer);
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

/** Interface for handling postMessage requests. */
export interface PostMessageRequestHandler {
  /**
   * Handles a 'raw' request.
   * If this throws an exception, it will be sent back in the response. This
   * supports built-in error types like Error, as well as ErrorWithReasonImpl.
   *
   * @param type The request type, from request_types.ts.
   * @param payload The payload, from request_types.ts.
   * @returns The response to be returned to the client.
   */
  handleRawRequest(type: string, payload: any, extras: ResponseExtras):
      Promise<{
        /** The payload of the response. */
        payload: any,
      }|undefined>;

  /** Called when each request is received. */
  onRequestReceived(type: string): void;
  /** Called when a request handler throws an exception. */
  onRequestHandlerException(type: string): void;
  /**
   * Called when a request response is sent (will not be called if
   * `onRequestHandlerException()` is called.).
   */
  onRequestCompleted(type: string): void;
}

// Receives requests over postMessage and forward them to a
// `PostMessageRequestHandler`.
export class PostMessageRequestReceiver {
  constructor(
      private router: PostMessageRouter,
      public handler: PostMessageRequestHandler) {
    assert(router.receiver === undefined);
    router.receiver = this;
  }

  async onMessage(requestMessage: RequestMessage) {
    const {senderId, requestId, type, requestPayload} = requestMessage;
    let response;
    let exception: TransferableException|undefined;
    const extras = new ResponseExtras();
    this.handler.onRequestReceived(type);
    this.router.maybeLogMessage(type, 'processing request', requestPayload);
    try {
      response =
          await this.handler.handleRawRequest(type, requestPayload, extras);
    } catch (error) {
      this.handler.onRequestHandlerException(type);
      console.warn('Unexpected error', error);
      if (error instanceof Error) {
        exception = newTransferableException(error);
      } else {
        exception =
            newTransferableException(new Error(`Unexpected error: ${error}`));
      }
    }

    if (!exception) {
      this.handler.onRequestCompleted(type);
    }

    // If the message contains no `requestId`, a response is not requested.
    if (!requestId) {
      return;
    }
    this.router.sendResponse(
        type, senderId, requestId, response?.payload, exception,
        extras.transfers);
  }
}

export function createBidirectionalPostMessageTransport(
    remoteOrigin: string,
    senderId: string,
    postMessageSender: PostMessageSender,
    handler: PostMessageRequestHandler,
    logPrefix: string,
) {
  const router = new PostMessageRouter(
      remoteOrigin, senderId, postMessageSender, logPrefix);
  const sender = new PostMessageRequestSender(router);
  const receiver = new PostMessageRequestReceiver(router, handler);
  // Note: receiver is returned to allow replacing the handler.
  return {router, sender, receiver};
}

// Converts a value to JSON for debug logging.
function toDebugJson(v: any): string {
  return JSON.stringify(v, (_key, value) => {
    // stringify throws on bigint, so convert it.
    if (typeof value === 'bigint') {
      return value.toString();
    }
    return value;
  });
}
