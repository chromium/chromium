// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
