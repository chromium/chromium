// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AllRequestTypes, AllRequestTypesWithoutReturn, RequestRequestType, RequestResponseType, TransferableException} from './request_types.js';
import {exceptionFromTransferable, newTransferableException} from './request_types.js';

// This file contains helpers to send and receive messages over postMessage.

// Requests sent over postMessage have this structure.
declare interface RequestMessage {
  // Unique ID of the sender.
  senderId: string;
  // Present for any Glic request message.
  glicRequest: true;
  // The type of request.
  type: string;
  // A unique ID of the request. Round-tripped in the response. `undefined` if a
  // response is not desired.
  requestId?: number;
  // A payload. Each type of request has a distinct payload type.
  requestPayload: any;
}

// Responses sent over postMessage have this structure. Responses are messages
// sent in response to a `RequestMessage`.
declare interface ResponseMessage {
  // Round-tripped RequestMessage.senderId.
  senderId: string;
  // The type of request.
  type: string;
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

// Sends requests over postMessage. Ideally this type would be parameterized by
// only one of HostRequestTypes or WebClientRequestTypes, but typescript
// cannot represent this. Instead, this class can send messages of any type.
export class PostMessageRequestSender extends MessageLogger {
  requestId = 1;
  responseHandlers: Map<number, (response: ResponseMessage) => void> =
      new Map();
  onDestroy: () => void;

  constructor(
      private messageSender: PostMessageSender, private remoteOrigin: string,
      private senderId: string, logPrefix: string) {
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

  // Handles responses from the host.
  private onMessage(event: MessageEvent) {
    // Ignore all messages that don't look like responses.
    if (event.origin !== this.remoteOrigin ||
        event.data.senderId !== this.senderId ||
        event.data.type === undefined || event.data.responseId === undefined) {
      return;
    }
    const response = event.data as ResponseMessage;
    const handler = this.responseHandlers.get(response.responseId);
    if (!handler) {
      // No handler for this request.
      return;
    }
    this.responseHandlers.delete(response.responseId);
    handler(response);
  }

  // Sends a request to the host, and returns a promise that resolves with its
  // response.
  requestWithResponse<T extends keyof AllRequestTypes>(
      requestType: T, request: RequestRequestType<T>,
      transfer: Transferable[] = []): Promise<RequestResponseType<T>> {
    const {promise, resolve, reject} =
        Promise.withResolvers<RequestResponseType<T>>();
    const requestId = this.requestId++;
    this.responseHandlers.set(requestId, (response: ResponseMessage) => {
      if (response.exception !== undefined) {
        this.maybeLogMessage(
            requestType, 'received with exception', response.exception);
        reject(exceptionFromTransferable(response.exception));
      } else {
        this.maybeLogMessage(requestType, 'received', response.responsePayload);
        resolve(response.responsePayload as RequestResponseType<T>);
      }
    });

    this.maybeLogMessage(requestType, 'sending', request);
    const message: RequestMessage = {
      senderId: this.senderId,
      glicRequest: true,
      requestId,
      type: requestType,
      requestPayload: request,
    };
    this.messageSender.postMessage(message, this.remoteOrigin, transfer);
    return promise;
  }

  // Sends a request to the host, and does not track the response.
  requestNoResponse<T extends keyof AllRequestTypesWithoutReturn>(
      requestType: T, request: RequestRequestType<T>,
      transfer: Transferable[] = []): void {
    const message: RequestMessage = {
      senderId: this.senderId,
      glicRequest: true,
      requestId: undefined,
      type: requestType,
      requestPayload: request,
    };
    this.maybeLogMessage(requestType, 'sending', request);
    this.messageSender.postMessage(message, this.remoteOrigin, transfer);
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
export class PostMessageRequestReceiver extends MessageLogger {
  private onDestroy: () => void;
  constructor(
      private embeddedOrigin: string, senderId: string,
      private postMessageSender: PostMessageSender,
      private handler: PostMessageRequestHandler, logPrefix: string) {
    super(senderId, logPrefix);
    const handlerFunction = this.onMessage.bind(this);
    window.addEventListener('message', handlerFunction);
    this.onDestroy = () => {
      window.removeEventListener('message', handlerFunction);
    };
  }

  destroy() {
    this.onDestroy();
  }

  async onMessage(event: MessageEvent) {
    // This receives all messages to the window, so ignore them if they don't
    // look compatible.
    if (event.origin !== this.embeddedOrigin || !event.source ||
        !event.data.glicRequest) {
      return;
    }
    const requestMessage = event.data as RequestMessage;
    const {requestId, type, requestPayload, senderId} = requestMessage;
    let response;
    let exception: TransferableException|undefined;
    const extras = new ResponseExtras();
    this.handler.onRequestReceived(type);
    this.maybeLogMessage(type, 'processing request', requestPayload);
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
    this.maybeLogMessage(type, 'sending response', response?.payload);
    const responseMessage: ResponseMessage = {
      type,
      responseId: requestId,
      responsePayload: response?.payload,
      senderId,
    };
    if (exception) {
      responseMessage.exception = exception;
    }
    this.postMessageSender.postMessage(
        responseMessage,
        this.embeddedOrigin,
        extras.transfers,
    );
  }
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
