// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CLICK_NEXT_BUTTON, ClickNextButtonEvent, createCustomEvent, DISABLE_ALL_BUTTONS, DISABLE_NEXT_BUTTON, DisableAllButtonsEvent, DisableNextButtonEvent, ENABLE_ALL_BUTTONS, EnableAllButtonsEvent, TRANSITION_STATE, TransitionStateEvent} from './events.js';
import {RmadErrorCode, StateResult} from './shimless_rma.mojom-webui.js';

declare global {
  interface HTMLElementEventMap {
    [DISABLE_NEXT_BUTTON]: DisableNextButtonEvent;
    [DISABLE_ALL_BUTTONS]: DisableAllButtonsEvent;
    [ENABLE_ALL_BUTTONS]: EnableAllButtonsEvent;
    [TRANSITION_STATE]: TransitionStateEvent;
    [CLICK_NEXT_BUTTON]: ClickNextButtonEvent;
  }
}

function makeElementTabbable(element: HTMLElement): void {
  element.setAttribute('tabindex', '0');
}

function removeElementFromKeyboardNavigation(element: HTMLElement): void {
  element.setAttribute('tabindex', '-1');
}

/**
 * Disables the next button from being clicked.
 */
export function disableNextButton(element: HTMLElement): void {
  element.dispatchEvent(createCustomEvent(DISABLE_NEXT_BUTTON, true));
}

/**
 * Enables the next button to be clicked.
 */
export function enableNextButton(element: HTMLElement): void {
  element.dispatchEvent(createCustomEvent(DISABLE_NEXT_BUTTON, false));
}

/**
 * Disables all inputs on the page.
 */
export function disableAllButtons(
    element: HTMLElement, showBusyStateOverlay: boolean): void {
  element.dispatchEvent(
      createCustomEvent(DISABLE_ALL_BUTTONS, {showBusyStateOverlay}));
}

/**
 * Enables all inputs on the page.
 */
export function enableAllButtons(element: HTMLElement): void {
  element.dispatchEvent(createCustomEvent(ENABLE_ALL_BUTTONS, {}));
}

/**
 * Dispatches an event captured by shimless_rma.js that will execute `fn`,
 * process the result, then transition to the next state.
 */
export function executeThenTransitionState(
    element: HTMLElement,
    fn: () =>
        Promise<{stateResult: StateResult, error?: RmadErrorCode}>): void {
  element.dispatchEvent(createCustomEvent(TRANSITION_STATE, fn));
}

/**
 * Dispatches an event to click the next button.
 */
export function dispatchNextButtonClick(element: HTMLElement): void {
  element.dispatchEvent(createCustomEvent(CLICK_NEXT_BUTTON, {}));
}

/**
 * Make the first non-disabled component in the list tabbable
 * and remove the remaining components from keyboard navigation.
 */
export function modifyTabbableElement(
    element: HTMLElement, isFirstClickableComponent: boolean): void {
  isFirstClickableComponent ? makeElementTabbable(element) :
                              removeElementFromKeyboardNavigation(element);
}

/**
 * Sets the focus on the page title.
 */
export function focusPageTitle(element: HTMLElement): void {
  const pageTitle: HTMLHeadingElement|null =
      element!.shadowRoot!.querySelector('h1');
  assert(pageTitle);
  afterNextRender(element, () => {
    pageTitle.focus();
  });
}
