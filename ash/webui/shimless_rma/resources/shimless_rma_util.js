// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {StateResult} from './shimless_rma_types.js';

/**
 * @param {!HTMLElement} element
 */
function makeElementTabbable(element) {
  element.setAttribute('tabindex', '0');
}

/**
 * @param {!HTMLElement} element
 */
function removeElementFromKeyboardNavigation(element) {
  element.setAttribute('tabindex', '-1');
}

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
 * @param {!function(): !Promise<!{stateResult: !StateResult}>} fn
 */
export function executeThenTransitionState(element, fn) {
  element.dispatchEvent(new CustomEvent(
      'transition-state',
      {bubbles: true, composed: true, detail: fn},
      ));
}

/**
 * Dispatches an event to click the next button.
 * @param {!HTMLElement} element
 */
export function dispatchNextButtonClick(element) {
  element.dispatchEvent(new CustomEvent('click-next-button', {
    bubbles: true,
    composed: true,
  }));
}

/**
 * Make the first non-disabled component in the list tabbable
 * and remove the remaining components from keyboard navigation.
 * @param {!HTMLElement} element
 * @param {boolean} isFirstClickableComponent
 */
export function modifyTabbableElement(element, isFirstClickableComponent) {
  isFirstClickableComponent ? makeElementTabbable(element) :
                              removeElementFromKeyboardNavigation(element);
}

/**
 * Sets the focus on the page title.
 * @param {!HTMLElement} element
 */
export function focusPageTitle(element) {
  const pageTitle = element.shadowRoot.querySelector('h1');
  assert(pageTitle);
  afterNextRender(element, () => {
    pageTitle.focus();
  });
}
