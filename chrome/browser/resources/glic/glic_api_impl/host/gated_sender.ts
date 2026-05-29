// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ObservableValue} from '../../observable.js';
import type {PostMessageRemote, RequestPayload, ResponsePayload} from '../transport/post_message_transport.js';

interface QueuedMessage {
  order: number;
  requestType: string;
  payload: unknown;
  transfer: Transferable[];
}
type IfHasNoResponseType<MapType, T extends keyof MapType> =
    ResponsePayload<MapType, T> extends void ? T : never;
type IfUngated<MapType, T extends keyof MapType> =
    MapType[T] extends {backgroundAllowed: true} ? T : never;

type UngatedRequests<MapType> =
keyof{[K in keyof MapType as IfUngated<MapType, K>]: void};
type RequestsWithNoResponse<MapType> =
keyof{[K in keyof MapType as IfHasNoResponseType<MapType, K>]: void};

// Sends messages to the client, subject to the `backgroundAllowed` property.
// Supports queueing of messages not `backgroundAllowed`.
export class GatedSender<MapType> {
  private sequenceNumber = 0;
  private messageQueue: QueuedMessage[] = [];
  private keyedMessages = new Map<string, QueuedMessage>();
  constructor(
      private sender: PostMessageRemote<MapType>,
      private shouldGate: ObservableValue<boolean>) {
    this.shouldGate.subscribe(this.setGating.bind(this));
  }

  isGating(): boolean {
    return this.shouldGate.getCurrentValue()!;
  }

  // This is an escape hatch which should be used sparingly.
  getRawSender(): PostMessageRemote<MapType> {
    return this.sender;
  }

  private setGating(shouldGateRequests: boolean): void {
    if (shouldGateRequests) {
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
          message.requestType as keyof MapType, message.payload as never,
          message.transfer);
    });
  }

  // Sends a request whenever glic is active.
  // Queues the request for later if glic is backgrounded.
  sendWhenActive<T extends keyof MapType>(
      requestType: IfHasNoResponseType<MapType, T>,
      request: RequestPayload<MapType, T>,
      transfer: Transferable[] = []): void {
    if (!this.isGating()) {
      this.sender.requestNoResponse(requestType, request, transfer);
    } else {
      this.messageQueue.push({
        order: this.sequenceNumber++,
        requestType: requestType as string,
        payload: request,
        transfer,
      });
    }
  }

  // Sends a request only if glic is active, otherwise it is dropped.
  sendIfActiveOrDrop<T extends RequestsWithNoResponse<MapType>&keyof MapType>(
      requestType: T, request: RequestPayload<MapType, T>,
      transfer: Transferable[] = []): void {
    if (!this.isGating()) {
      this.sender.requestNoResponse(requestType, request, transfer);
    }
  }

  // Sends a request if glic is active, otherwise the request is queued for
  // later. If more than one request has the same key
  // `${requestType},${additionalKey}`, only the last request is saved in the
  // queue.
  sendLatestWhenActive<T extends keyof MapType>(
      requestType: IfHasNoResponseType<MapType, T>,
      request: RequestPayload<MapType, T>, transfer: Transferable[] = [],
      additionalKey?: string): void {
    if (!this.isGating()) {
      this.sender.requestNoResponse(requestType, request, transfer);
    } else {
      let key: string = requestType as string;
      if (additionalKey) {
        key += ',' + additionalKey;
      }
      this.keyedMessages.set(key, {
        order: this.sequenceNumber++,
        requestType: requestType as string,
        payload: request,
        transfer,
      });
    }
  }

  // Sends a request without waiting for a response. Allowed only for
  // backgroundAllowed request types.
  requestNoResponse<T extends keyof MapType>(
      requestType: IfUngated<MapType, T>, request: RequestPayload<MapType, T>,
      transfer: Transferable[] = []): void {
    this.sender.requestNoResponse(requestType, request, transfer);
  }

  // Sends a request and waits for a response. Allowed only for
  // backgroundAllowed request types.
  requestWithResponse<T extends UngatedRequests<MapType>>(
      requestType: T, request: RequestPayload<MapType, T>,
      transfer: Transferable[] = []): Promise<ResponsePayload<MapType, T>> {
    return this.sender.requestWithResponse(requestType, request, transfer);
  }
}
