// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';


/**
 * Queries |selector| on |element|'s shadow root and returns the resulting
 * element if there is any.
 */
export function $$<K extends keyof HTMLElementTagNameMap>(
    element: Element, selector: K): HTMLElementTagNameMap[K]|null;
export function $$<K extends keyof SVGElementTagNameMap>(
    element: Element, selector: K): SVGElementTagNameMap[K]|null;
export function $$<E extends Element = Element>(
    element: Element, selector: string): E|null;
export function $$(element: Element, selector: string) {
  return element.shadowRoot!.querySelector(selector);
}

type Constructor<T> = new (...args: any[]) => T;

/**
 * Queries |selector| on |root| and returns the resulting element. Throws
 * exception if there is no resulting element or if element is not of type
 * |type|.
 */
export function strictQuery<T>(
    root: Element|ShadowRoot, selector: string, type: Constructor<T>): T {
  const element = root.querySelector(selector);
  assert(element && element instanceof type);
  return element;
}

/**
 * The enum value sent as part of action-chips-retrieval-state-changed.
 * The handler of the event should expect to receive UPDATED multiple times.
 */
export enum ActionChipsRetrievalState {
  // The initial state. This is not sent as part of the event and can be used as
  // the default value of a variable containing this enum.
  INITIAL,
  // The state used in an event firing when the first and only retrieval
  // request is sent from this component.
  REQUESTED,
  // The state used in events firing when the action chips are updated by a call
  // from the browser side.
  UPDATED,
}
