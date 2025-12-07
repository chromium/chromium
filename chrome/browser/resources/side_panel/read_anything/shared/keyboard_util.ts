// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from '//resources/js/util.js';

const FORWARD_ARROWS = ['ArrowRight', 'ArrowDown'];
const BACKWARD_ARROWS = ['ArrowLeft', 'ArrowUp'];
const ALL_ARROWS = BACKWARD_ARROWS.concat(FORWARD_ARROWS);
const HORIZONTAL_ARROWS = ['ArrowRight', 'ArrowLeft'];

// Returns the next item to focus in the list of focusableElements, depending
// on which key is used and whether the UI is LTR or RTL.
export function getNewIndex(
    key: string, target: HTMLElement,
    focusableElements: HTMLElement[]): number {
  let currentIndex = focusableElements.indexOf(target);
  if (!isArrow(key)) {
    return currentIndex;
  }
  const direction = isForwardArrow(key) ? 1 : -1;
  // If target wasn't found in focusable elements, and we're going
  // backwards, adjust currentIndex so we move to the last focusable element
  if (currentIndex === -1 && direction === -1) {
    currentIndex = focusableElements.length;
  }
  // Move to the next focusable item in the menu, wrapping around
  // if we've reached the end or beginning.
  return (currentIndex + direction + focusableElements.length) %
      focusableElements.length;
}

export function isArrow(key: string): boolean {
  return ALL_ARROWS.includes(key);
}

export function isForwardArrow(key: string): boolean {
  return (isRTL() ? BACKWARD_ARROWS : FORWARD_ARROWS).includes(key);
}

export function isHorizontalArrow(key: string): boolean {
  return HORIZONTAL_ARROWS.includes(key);
}
