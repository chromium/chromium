// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Given a |container| that has scrollable content, <div>'s before and after the
 * |container| are created with an attribute "scroll-border". These <div>'s are
 * updated to have an attribute "show" when there is more content in the
 * direction of the "scroll-border". Styling is left to the caller.
 *
 * Returns an |IntersectionObserver| so the caller can disconnect the observer
 * when needed.
 * @param {!Element} container
 * @param {!Element} topBorder
 * @param {!Element} bottomBorder
 * @param {string} showAttribute
 * @return {!IntersectionObserver}
 */
export function createScrollBorders(
    container, topBorder, bottomBorder, showAttribute) {
  const topProbe = document.createElement('div');
  container.prepend(topProbe);
  const bottomProbe = document.createElement('div');
  container.append(bottomProbe);
  const observer = new IntersectionObserver(entries => {
    entries.forEach(({target, intersectionRatio}) => {
      const show = intersectionRatio === 0;
      if (target === topProbe) {
        topBorder.toggleAttribute(showAttribute, show);
      } else if (target === bottomProbe) {
        bottomBorder.toggleAttribute(showAttribute, show);
      }
    });
  }, {root: container});
  observer.observe(topProbe);
  observer.observe(bottomProbe);
  return observer;
}

/**
 * Converts a mojoBase.mojom.String16 to a JavaScript String.
 * @param {?mojoBase.mojom.String16} str
 * @return {string}
 */
export function decodeString16(str) {
  return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
}

/**
 * Converts a JavaScript String to a mojoBase.mojom.String16.
 * @param {string} str
 * @return {!mojoBase.mojom.String16}
 */
export function mojoString16(str) {
  const array = new Array(str.length);
  for (let i = 0; i < str.length; ++i) {
    array[i] = str.charCodeAt(i);
  }
  return {data: array};
}

/**
 * Converts a time delta in milliseconds to mojoBase.mojom.TimeDelta.
 * @param {number} timeDelta time delta in milliseconds
 * @returns {!mojoBase.mojom.TimeDelta}
 */
export function mojoTimeDelta(timeDelta) {
  return {microseconds: Math.floor(timeDelta * 1000)};
}

/**
 * Queries |selector| on |element|'s shadow root and returns the resulting
 * element if there is any.
 * @param {!Element} element
 * @param {string} selector
 * @return {Element}
 */
export function $$(element, selector) {
  return element.shadowRoot.querySelector(selector);
}
