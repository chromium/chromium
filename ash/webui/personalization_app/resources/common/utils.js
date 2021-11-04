// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be shared between trusted and untrusted
 * code.
 */

/**
 * @param {string} layout
 * @return {ash.personalizationApp.mojom.WallpaperLayout}
 */
export function getWallpaperLayoutEnum(layout) {
  switch (layout) {
    case 'FILL':
      return ash.personalizationApp.mojom.WallpaperLayout.kCenterCropped;
    case 'CENTER': // fall through
    default:
      return ash.personalizationApp.mojom.WallpaperLayout.kCenter;
  }
}

/**
 * Checks if argument is an array with non-zero length.
 * @param {?Object} maybeArray
 * @return {boolean}
 */
export function isNonEmptyArray(maybeArray) {
  return Array.isArray(maybeArray) && maybeArray.length > 0;
}

/**
 * Checks if argument is null or is an array.
 * @param {?Array} maybeNullOrArray
 * @return {boolean}
 */
export function isNullOrArray(maybeNullOrArray) {
  return maybeNullOrArray === null || Array.isArray(maybeNullOrArray);
}

/**
 * Checks if argument is null or is a number.
 * @param {?number} maybeNullOrNumber
 * @return {boolean}
 */
export function isNullOrNumber(maybeNullOrNumber) {
  return maybeNullOrNumber === null || typeof maybeNullOrNumber === 'number';
}

/**
 * Attach a listener to a child element onload function. Returns a promise
 * that resolves when that child element is loaded.
 * @param {*} element A polymer element
 * @param {!string} id Id of the child element.
 * @param {function(*, function(...*): void, !Array=): void}
 *     afterNextRender callback for first render of element.
 * @return {!Promise<!HTMLElement>}
 */
export function promisifyOnload(element, id, afterNextRender) {
  const promise = new Promise((resolve) => {
    function readyCallback() {
      const child = element.shadowRoot.getElementById(id);
      child.onload = () => resolve(child);
    }
    afterNextRender(element, readyCallback);
  });
  return promise;
}

/**
 * Returns true if this event is a user action to select an item.
 * @param {!Event} event
 * @return {boolean}
 */
export function isSelectionEvent(event) {
  return (event instanceof MouseEvent && event.type === 'click') ||
      (event instanceof KeyboardEvent && event.key === 'Enter');
}

/**
 * Sets a css variable to control the animation delay.
 * @param {number} index
 * @return {string}
 */
export function getLoadingPlaceholderAnimationDelay(index) {
  return `--animation-delay: ${index * 83}ms;`;
}

/**
 * Returns the number of grid items to render per row given the current inner
 * width of the |window|.
 * @return {number}
 */
export function getNumberOfGridItemsPerRow() {
  return window.innerWidth > 688 ? 4 : 3;
}

/**
 * Normalizes the given |key| for RTL.
 * @param {string} key
 * @param {boolean} isRTL
 * @return {string}
 */
export function normalizeKeyForRTL(key, isRTL) {
  if (isRTL) {
    if (key === 'ArrowLeft') {
      return 'ArrowRight';
    }
    if (key === 'ArrowRight') {
      return 'ArrowLeft';
    }
  }
  return key;
}
