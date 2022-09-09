// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates an element named |elementName| containing the content |text|.
 * @param elementName Name of the new element to be created.
 * @param text Text to be contained in the new element.
 * @param attributes Optional attribute dictionary for the
 *     element.
 * @return The newly created HTML element.
 */
export function createElementFromText(
    elementName: string, text: string,
    attributes?: {[key: string]: string}): HTMLElement {
  const element = document.createElement(elementName);
  element.appendChild(document.createTextNode(text));
  if (attributes) {
    for (const key in attributes) {
      element.setAttribute(key, attributes[key]!);
    }
  }
  return element;
}

/**
 * Creates an element with |tagName| containing the content |dict|.
 * @param elementName Name of the new element to be created.
 * @param dict Dictionary to be contained in the new element.
 * @return The newly created HTML element.
 */
export function createElementFromDictionary(
    elementName: string, dict: {[key: string]: string}): HTMLElement {
  const element = document.createElement(elementName);
  for (const key in dict) {
    element.appendChild(document.createTextNode(key + ': ' + dict[key]));
    element.appendChild(document.createElement('br'));
  }
  return element;
}
