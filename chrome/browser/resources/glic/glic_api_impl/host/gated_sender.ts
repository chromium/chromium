// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ObservableValue} from '../../observable.js';
import type {InterfaceDef, InterfaceDefMethods, PostMessageRemote, RequestPayload, ResponsePayload} from '../transport/post_message_transport.js';

interface QueuedMessage {
  order: number;
  requestType: string;
  payload: unknown;
  transfer: Transferable[];
}
type IfHasNoResponseType<I extends InterfaceDef,
                                   T extends keyof InterfaceDefMethods<I>> =
    ResponsePayload<InterfaceDefMethods<I>, T> extends void ? T : never;
type IfUngated<I extends InterfaceDef, T extends keyof InterfaceDefMethods<I>> =
    InterfaceDefMethods<I>[T] extends {backgroundAllowed: true} ? T : never;

type UngatedRequests<I extends InterfaceDef> =
keyof{[K in keyof InterfaceDefMethods<I>as IfUngated<I, K>]: void};
type RequestsWithNoResponse<I extends InterfaceDef> =
keyof{[K in keyof InterfaceDefMethods<I>as IfHasNoResponseType<I, K>]: void};

// Sends messages to the client, subject to the `backgroundAllowed` property.
// Supports queueing of messages not `backgroundAllowed`.
export class GatedSender<I extends InterfaceDef> {
  private sequenceNumber = 0;
  private messageQueue: QueuedMessage[] = [];
  private keyedMessages = new Map<string, QueuedMessage>();
  constructor(
      private sender: PostMessageRemote<I>,
      private shouldGate: ObservableValue<boolean>) {
    this.shouldGate.subscribe(this.setGating.bind(this));
  }

  isGating(): boolean {
    return this.shouldGate.getCurrentValue()!;
  }

  // This is an escape hatch which should be used sparingly.
  getRawSender(): PostMessageRemote<I> {
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
          message.requestType as keyof InterfaceDefMethods<I>,
          message.payload as never, message.transfer);
    });
  }

  // Sends a request whenever glic is active.
  // Queues the request for later if glic is backgrounded.
  sendWhenActive<T extends keyof InterfaceDefMethods<I>>(
      requestType: IfHasNoResponseType<I, T>,
      request: RequestPayload<InterfaceDefMethods<I>, T>,
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
  sendIfActiveOrDrop<T extends RequestsWithNoResponse<I>&
                     keyof InterfaceDefMethods<I>>(
      requestType: T, request: RequestPayload<InterfaceDefMethods<I>, T>,
      transfer: Transferable[] = []): void {
    if (!this.isGating()) {
      this.sender.requestNoResponse(requestType, request, transfer);
    }
  }

  // Sends a request if glic is active, otherwise the request is queued for
  // later. If more than one request has the same key
  // `${requestType},${additionalKey}`, only the last request is saved in the
  // queue.
  sendLatestWhenActive<T extends keyof InterfaceDefMethods<I>>(
      requestType: IfHasNoResponseType<I, T>,
      request: RequestPayload<InterfaceDefMethods<I>, T>,
      transfer: Transferable[] = [], additionalKey?: string): void {
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
  requestNoResponse<T extends keyof InterfaceDefMethods<I>>(
      requestType: IfUngated<I, T>,
      request: RequestPayload<InterfaceDefMethods<I>, T>,
      transfer: Transferable[] = []): void {
    this.sender.requestNoResponse(requestType, request, transfer);
  }

  // Sends a request and waits for a response. Allowed only for
  // backgroundAllowed request types.
  requestWithResponse<T extends UngatedRequests<I>>(
      requestType: T, request: RequestPayload<InterfaceDefMethods<I>, T>,
      transfer: Transferable[] = []):
      Promise<ResponsePayload<InterfaceDefMethods<I>, T>> {
    return this.sender.requestWithResponse(requestType, request, transfer);
  }
}
