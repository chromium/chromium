// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StateResult} from './shimless_rma_types.js';

/**
 * Disables the next button from being clicked.
 * @param {!HTMLElement} element
 */
export function disableNextButton(element) {
  element.dispatchEvent(new CustomEvent(
      'disable-next-button',
      {bubbles: true, composed: true, detail: true},
      ));
}

/**
 * Enables the next button to be clicked.
 * @param {!HTMLElement} element
 */
export function enableNextButton(element) {
  element.dispatchEvent(new CustomEvent(
      'disable-next-button',
      {bubbles: true, composed: true, detail: false},
      ));
}

/**
 * Disables all inputs on the page.
 * @param {!HTMLElement} element
 * @param {boolean} showBusyStateOverlay
 */
export function disableAllButtons(element, showBusyStateOverlay) {
  element.dispatchEvent(new CustomEvent(
      'disable-all-buttons',
      {bubbles: true, composed: true, detail: {showBusyStateOverlay}},
      ));
}

/**
 * Enables all inputs on the page.
 * @param {!HTMLElement} element
 */
export function enableAllButtons(element) {
  element.dispatchEvent(new CustomEvent(
      'enable-all-buttons',
      {
        bubbles: true,
        composed: true,
      },
      ));
}

/**
 * Dispatches an event captured by shimless_rma.js that will execute `fn`,
 * process the result, then transition to the next state.
 * @param {!HTMLElement} element
 * @param {!function(): !Promise<!StateResult>} fn
 */
export function executeThenTransitionState(element, fn) {
  element.dispatchEvent(new CustomEvent(
      'transition-state',
      {bubbles: true, composed: true, detail: fn},
      ));
}
