/* Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
const CLASSES = {
  MAC: 'mac',                            // Applies MacOS specific properties.
  WIN: 'win',                            // Applies Windows specific properties.
  MOUSE_NAVIGATION: 'mouse-navigation',  // Removes blue focus ring.
};

/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {Element} The found element or null if not found.
 */
function $(id) {
  // eslint-disable-next-line no-restricted-properties
  return document.getElementById(id);
}

/**
 * Contains common functions used in the main NTP page and its iframes.
 */
const utils = {};

/**
 * Disables the focus outline for |element| on mousedown.
 * @param {Element} element The element to remove the focus outline from.
 */
utils.disableOutlineOnMouseClick = function(element) {
  element.addEventListener('mousedown', () => {
    element.classList.add(CLASSES.MOUSE_NAVIGATION);
    element.addEventListener('blur', () => {
      element.classList.remove(CLASSES.MOUSE_NAVIGATION);
    }, {once: true});
  });
};

/**
 * Returns whether the given URL has a known, safe scheme.
 * @param {string} url URL to check.
 */
utils.isSchemeAllowed = function(url) {
  return url.startsWith('http://') || url.startsWith('https://') ||
      url.startsWith('ftp://') || url.startsWith('chrome-extension://');
};

/**
 * Sets CSS class for |element| corresponding to the current platform.
 * @param {Element} element The element to set the current platform.
 */
utils.setPlatformClass = function(element) {
  element.classList.toggle(
      CLASSES.WIN, navigator.userAgent.indexOf('Windows') > -1);
  element.classList.toggle(
      CLASSES.MAC, navigator.userAgent.indexOf('Mac') > -1);
};
