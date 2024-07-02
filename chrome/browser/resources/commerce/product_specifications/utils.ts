// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from 'chrome://resources/js/assert.js';

export interface UrlListEntry {
  title: string;
  url: string;
  imageUrl: string;
}

export function getAbbreviatedUrl(urlString: string) {
  const url = new URL(urlString);
  // Chrome URLs should all have been filtered out.
  assert(url.protocol !== 'chrome:');
  return url.hostname;
}

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
