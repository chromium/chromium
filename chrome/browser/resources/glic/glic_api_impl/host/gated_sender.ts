// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PostMessageRequestSender} from 'glic_api_impl/post_message_transport.js';
import type {AllRequestTypesWithoutReturn, AllRequestTypesWithReturn, RequestRequestType, WebClientRequestTypes} from 'glic_api_impl/request_types.js';

interface QueuedMessage {
  order: number;
  requestType: string;
  payload: any;
  transfer: Transferable[];
}

type IsGatedRequest<T extends keyof WebClientRequestTypes> =
    'backgroundAllowed' extends keyof WebClientRequestTypes[T] ? false : true;
export type UngatedWebClientRequestTypes = {
  [Property in keyof WebClientRequestTypes as
       IsGatedRequest<Property> extends true ? never : Property]: true;
};

// Sends messages to the client, subject to the `backgroundAllowed` property.
// Supports queueing of messages not `backgroundAllowed`.
export class GatedSender {
  private sequenceNumber = 0;
  private messageQueue: QueuedMessage[] = [];
  private keyedMessages = new Map<string, QueuedMessage>();
  private shouldGateRequests = true;
  constructor(private sender: PostMessageRequestSender) {}

  isGating(): boolean {
    return this.shouldGateRequests;
  }

  // This is an escape hatch which should be used sparingly.
  getRawSender(): PostMessageRequestSender {
    return this.sender;
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
