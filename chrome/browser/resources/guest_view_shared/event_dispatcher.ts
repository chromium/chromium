// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNonNull} from '//resources/js/assert.js';
import type {DictionaryValue} from '//resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {BrowserProxyImpl} from './slim_web_view_browser_proxy.js';

export class EventDict {
  constructor(private data: DictionaryValue) {}

  getString(key: string): string {
    assertNonNull(this.data.storage[key]?.stringValue);
    return this.data.storage[key].stringValue;
  }

  getBool(key: string): boolean {
    assertNonNull(this.data.storage[key]?.boolValue);
    return this.data.storage[key].boolValue;
  }

  getInt(key: string): number {
    assertNonNull(this.data.storage[key]?.intValue);
    return this.data.storage[key].intValue;
  }

  getDict(key: string): EventDict {
    assertNonNull(this.data.storage[key]?.dictionaryValue);
    return new EventDict(this.data.storage[key].dictionaryValue);
  }
}

/**
 * Descriptor for an event.
 *
 * If `guestInstanceAssociated` is true, the received id is matched the guest
 * id, otherwise it is matched against the element id.
 *
 * If `factory` is set, it is called with the event arguments and should return
 * the event to be dispatched. If the factory returns null, the event is not
 * dispatched to the element.
 *
 * If `handler` is set, it is called bound to the event and with the element as
 * an argument. It is the responsibility of the handler to dispatch the event.
 * If the `handler` is undefined, the event is dispatched to the element.
 *
 * The remaining properties are used to construct an Event if the `factory` is
 * undefined.
 */
export interface EventDescriptor {
  guestInstanceAssociated?: boolean;
  factory?: (args: EventDict, guestInstanceId: number) => Event | null;
  handler?: (element: HTMLElement) => void;

  // Properties used if `factory` is undefined.
  cancelable?: boolean;
}

export type EventMap = Map<string, EventDescriptor>;

/**
 * Dispatches events to the given element if the instance ID matches.
 * Events are configured via the EventMap, which maps event names to
 * EventDescriptors.
 *
 * Elements that use this class should call connect() when they are connected to
 * the DOM and disconnect() when they are disconnected.
 */
export class EventDispatcher {
  private readonly eventDescriptors: EventMap;
  private readonly elementInstanceId: number;
  private readonly guestInstanceId: number;
  private readonly element: HTMLElement;
  private listenerId: number|null = null;

  constructor(
      eventDescriptors: EventMap, elementInstanceId: number,
      guestInstanceId: number, element: HTMLElement) {
    for (const [eventName, descriptor] of eventDescriptors) {
      assert(
          descriptor.cancelable === undefined ||
              descriptor.factory === undefined,
          `Event descriptor for "${
              eventName}" cannot specify both cancelable and factory`);
    }
    this.eventDescriptors = eventDescriptors;
    this.elementInstanceId = elementInstanceId;
    this.guestInstanceId = guestInstanceId;
    this.element = element;
  }

  connect() {
    assert(this.listenerId === null);
    const proxy = BrowserProxyImpl.getInstance();
    this.listenerId = proxy.callbackRouter.dispatchEvent.addListener(
        this.dispatch.bind(this));
  }

  disconnect() {
    assert(this.listenerId !== null);
    const proxy = BrowserProxyImpl.getInstance();
    proxy.callbackRouter.removeListener(this.listenerId);
    this.listenerId = null;
  }

  private dispatch(
      eventName: string, args: DictionaryValue, instanceId: number) {
    const descriptor = this.eventDescriptors.get(eventName);
    if (!descriptor) {
      return;
    }
    const targetId = descriptor.guestInstanceAssociated ?
        this.guestInstanceId :
        this.elementInstanceId;
    if (instanceId !== targetId) {
      return;
    }
    let event: Event|null = null;
    if (descriptor.factory) {
      event = descriptor.factory(new EventDict(args), this.guestInstanceId);
    } else {
      event = new Event(eventName, {
        bubbles: true,
        cancelable: descriptor.cancelable,
      });
    }
    if (event === null) {
      return;
    }
    if (descriptor.handler) {
      descriptor.handler.call(event, this.element);
    } else {
      this.element.dispatchEvent(event);
    }
  }
}
