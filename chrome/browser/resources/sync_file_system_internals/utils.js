// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates an element named |elementName| containing the content |text|.
 * @param {string} elementName Name of the new element to be created.
 * @param {string} text Text to be contained in the new element.
 * @param {Object=} opt_attributes Optional attribute dictionary for the
 *     element.
 * @return {!HTMLElement} The newly created HTML element.
 */
export function createElementFromText(elementName, text, opt_attributes) {
  const element =
      /** @type {!HTMLElement} */ (document.createElement(elementName));
  element.appendChild(document.createTextNode(text));
  if (opt_attributes) {
    for (const key in opt_attributes) {
      element.setAttribute(key, opt_attributes[key]);
    }
  }
  return element;
}

/**
 * Creates an element with |tagName| containing the content |dict|.
 * @param {string} elementName Name of the new element to be created.
 * @param {Object<string>} dict Dictionary to be contained in the new
 * element.
 * @return {!HTMLElement} The newly created HTML element.
 */
export function createElementFromDictionary(elementName, dict) {
  const element =
      /** @type {!HTMLElement} */ (document.createElement(elementName));
  for (const key in dict) {
    element.appendChild(document.createTextNode(key + ': ' + dict[key]));
    element.appendChild(document.createElement('br'));
  }
  return element;
}
