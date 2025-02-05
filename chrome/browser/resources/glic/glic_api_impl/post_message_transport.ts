// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ErrorReasonTypes, ErrorWithReason} from 'glic_api/glic_api.js';

import type {HostRequestTypes, WebClientRequestTypes} from './request_types.js';

// This file contains helpers to send and receive messages over postMessage.

// TODO(crbug.com/379684723): Make sure that request IDs cannot be duplicated,
// even if the web client is recreated.

// Requests sent over postMessage have this structure.
declare interface RequestMessage {
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
  // The type of request.
  type: string;
  // The round-tripped `RequestMessage.requestId`.
  responseId: number;
  // A payload. Each type of response has a distinct payload type. Not set if
  // exception is set.
  responsePayload?: any;
  // An error that occurred during processing the request. If this is set,
  // responsePayload will not be set.
  exception?: Error;
  // If exception is set, this may be set to indicate that the exception
  // is a ErrorWithReason exception.
  exceptionReason?: ErrorWithReasonDetails;
}

/** Any ErrorWithReason<T>.reason type. */
type AnyErrorReasonType = ErrorReasonTypes[keyof ErrorReasonTypes];
/** Any ErrorWithReason type. */
type AnyErrorWithReasonType = ErrorWithReason<keyof ErrorReasonTypes>;
/** Sent in ResponseMessage to reconstruct the ErrorWithReason. */
interface ErrorWithReasonDetails {
  reason: AnyErrorReasonType;
  reasonType: keyof ErrorReasonTypes;
}

// Something that has postMessage() - probably a window or WindowProxy.
declare interface PostMessageSender {
  postMessage(message: any, targetOrigin: string, transfer?: Transferable[]):
      void;
}

type AllRequestTypes = HostRequestTypes&WebClientRequestTypes;

// Sends requests over postMessage. Ideally this type would be parameterized by
// only one of HostRequestTypes or WebClientRequestTypes, but typescript
// cannot represent this. Instead, this class can send messages of any type.
export class PostMessageRequestSender {
  requestId = 1;
  responseHandlers: Map<number, (response: ResponseMessage) => void> =
      new Map();
  onDestroy: () => void;

  constructor(
      private messageSender: PostMessageSender, private remoteOrigin: string) {
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
    if (event.origin !== this.remoteOrigin || event.data.type === undefined ||
        event.data.responseId === undefined) {
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
      requestType: T, request: AllRequestTypes[T]['request'],
      transfer: Transferable[] = []): Promise<AllRequestTypes[T]['response']> {
    const {promise, resolve, reject} =
        Promise.withResolvers<AllRequestTypes[T]['response']>();
    const requestId = this.requestId++;
    this.responseHandlers.set(requestId, (response: ResponseMessage) => {
      if (response.exception !== undefined) {
        // Error types are serializable, but they do not serialize all members.
        // If exceptionReason is provided, we use it to reconstruct a
        // ErrorWithReason by just setting additional fields after
        // serialization.
        if (response.exceptionReason) {
          const withReason = response.exception as AnyErrorWithReasonType;
          withReason.reason = response.exceptionReason.reason;
          withReason.reasonType = response.exceptionReason.reasonType;
        }
        reject(response.exception);
      } else {
        resolve(response.responsePayload as AllRequestTypes[T]['response']);
      }
    });

    const message: RequestMessage = {
      glicRequest: true,
      requestId,
      type: requestType,
      requestPayload: request,
    };
    this.messageSender.postMessage(message, this.remoteOrigin, transfer);
    return promise;
  }

  // Sends a request to the host, and does not track the response.
  requestNoResponse<T extends keyof AllRequestTypes>(
      requestType: T, request: AllRequestTypes[T]['request'],
      transfer: Transferable[] = []) {
    const message: RequestMessage = {
      glicRequest: true,
      requestId: undefined,
      type: requestType,
      requestPayload: request,
    };
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
  handleRawRequest(type: string, payload: any): Promise<{
    /** The payload of the response. */
    payload: any,
    /** Objects to be transferred over postMessage(). */
    transfer: Transferable[],
  }|undefined>;
}

// Receives requests over postMessage and forward them to a
// `PostMessageRequestHandler`.
export class PostMessageRequestReceiver {
  private onDestroy: () => void;
  constructor(
      private embeddedOrigin: string,
      private postMessageSender: PostMessageSender,
      private handler: PostMessageRequestHandler) {
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
    const {requestId, type, requestPayload} = requestMessage;
    let response;
    let exception: Error|undefined;
    let reasonDetails: ErrorWithReasonDetails|undefined;
    try {
      response = await this.handler.handleRawRequest(type, requestPayload);
    } catch (error) {
      console.error('Unexpected error', error);
      if (error instanceof Error) {
        exception = error;
        const [reasonType, reason] =
            [(error as any).reasonType, (error as any).reason];
        if (reasonType !== undefined && reason !== undefined) {
          reasonDetails = {reason, reasonType};
        }
      } else {
        exception = new Error(`Unexpected error: ${error}`);
      }
    }

    // If the message contains no `requestId`, a response is not requested.
    if (!requestId) {
      return;
    }
    const responseMessage: ResponseMessage = {
      type,
      responseId: requestId,
      responsePayload: response?.payload,
    };
    if (exception) {
      responseMessage.exception = exception;
      if (reasonDetails) {
        responseMessage.exceptionReason = reasonDetails;
      }
    }
    this.postMessageSender.postMessage(
        responseMessage,
        this.embeddedOrigin,
        response?.transfer,
    );
  }
}
