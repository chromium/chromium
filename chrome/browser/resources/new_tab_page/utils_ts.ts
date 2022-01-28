// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Event interface for dom-repeat event. */
// TODO(dpapad): Move this to a shared location and reuse across WebUIs.
export interface RepeaterEvent<M> extends Event {
  model: {
    item: M,
    index: number,
  };
}

/**
 * Queries |selector| on |element|'s shadow root and returns the resulting
 * element if there is any.
 * TODO(crbug.com/1273590): Delete when utils.js is converted to TypeScript.
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
