// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '//resources/ash/common/assert.js';

/**
 * Alias for document.getElementById. Found elements must be HTMLElements.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
export function $(id) {
  // Disable getElementById restriction here, since we are instructing other
  // places to re-use the $() that is defined here.
  // eslint-disable-next-line no-restricted-properties
  const el = document.getElementById(id);
  return el ? assertInstanceof(el, HTMLElement) : null;
}

/**
 * @return {?Element} The currently focused element (including elements that are
 *     behind a shadow root), or null if nothing is focused.
 */
export function getDeepActiveElement() {
  let a = document.activeElement;
  while (a && a.shadowRoot && a.shadowRoot.activeElement) {
    a = a.shadowRoot.activeElement;
  }
  return a;
}

/**
 * DEPRECATED (if using Polymer): Use IronA11yAnnouncer instead.
 * TODO(crbug.com/985410): Replace all existing usages and remove this function.
 * Add an accessible message to the page that will be announced to
 * users who have spoken feedback on, but will be invisible to all
 * other users. It's removed right away so it doesn't clutter the DOM.
 * @param {string} msg The text to be pronounced.
 */
export function announceAccessibleMessage(msg) {
  const element = document.createElement('div');
  element.setAttribute('aria-live', 'polite');
  element.style.position = 'fixed';
  element.style.left = '-9999px';
  element.style.height = '0px';
  element.innerText = msg;
  document.body.appendChild(element);
  window.setTimeout(function() {
    document.body.removeChild(element);
  }, 50);
}

/**
 * Check the directionality of the page.
 * @return {boolean} True if Chrome is running an RTL UI.
 */
export function isRTL() {
  return document.documentElement.dir === 'rtl';
}

/**
 * Get an element that's known to exist by its ID. We use this instead of just
 * calling getElementById and not checking the result because this lets us
 * satisfy the JSCompiler type system.
 * @param {string} id The identifier name.
 * @return {!HTMLElement} the Element.
 */
export function getRequiredElement(id) {
  return assertInstanceof(
      $(id), HTMLElement, 'Missing required element: ' + id);
}

/**
 * Creates a new URL which is the old URL with a GET param of key=value.
 * @param {string} url The base URL. There is no validation checking on the URL
 *     so it must be passed in a proper format.
 * @param {string} key The key of the param.
 * @param {string} value The value of the param.
 * @return {string} The new URL.
 */
export function appendParam(url, key, value) {
  const param = encodeURIComponent(key) + '=' + encodeURIComponent(value);

  if (url.indexOf('?') === -1) {
    return url + '?' + param;
  }
  return url + '&' + param;
}

/**
 * Creates an element of a specified type with a specified class name.
 * @param {string} type The node type.
 * @param {string} className The class name to use.
 * @return {Element} The created element.
 */
export function createElementWithClassName(type, className) {
  const elm = document.createElement(type);
  elm.className = className;
  return elm;
}

/**
 * transitionend does not always fire (e.g. when animation is aborted
 * or when no paint happens during the animation). This function sets up
 * a timer and emulate the event if it is not fired when the timer expires.
 * @param {!HTMLElement} el The element to watch for transitionend.
 * @param {number=} timeOut The maximum wait time in milliseconds for the
 *     transitionend to happen. If not specified, it is fetched from |el|
 *     using the transitionDuration style value.
 */
export function ensureTransitionEndEvent(el, timeOut) {
  if (timeOut === undefined) {
    const style = getComputedStyle(el);
    timeOut = parseFloat(style.transitionDuration) * 1000;

    // Give an additional 50ms buffer for the animation to complete.
    timeOut += 50;
  }

  let fired = false;
  el.addEventListener('transitionend', function f(e) {
    el.removeEventListener('transitionend', f);
    fired = true;
  });
  window.setTimeout(function() {
    if (!fired) {
      el.dispatchEvent(new CustomEvent('transitionend',
            {bubbles: true, composed: true}));
    }
  }, timeOut);
}

/**
 * Replaces '&', '<', '>', '"', and ''' characters with their HTML encoding.
 * @param {string} original The original string.
 * @return {string} The string with all the characters mentioned above replaced.
 */
export function HTMLEscape(original) {
  return original.replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
}

/**
 * Quote a string so it can be used in a regular expression.
 * @param {string} str The source string.
 * @return {string} The escaped string.
 */
export function quoteString(str) {
  return str.replace(/([\\\.\+\*\?\[\^\]\$\(\)\{\}\=\!\<\>\|\:])/g, '\\$1');
}

/**
 * Calls |callback| and stops listening the first time any event in |eventNames|
 * is triggered on |target|.
 * @param {!EventTarget} target
 * @param {!Array<string>|string} eventNames Array or space-delimited string of
 *     event names to listen to (e.g. 'click mousedown').
 * @param {function(!Event)} callback Called at most once. The
 *     optional return value is passed on by the listener.
 */
export function listenOnce(target, eventNames, callback) {
  if (!Array.isArray(eventNames)) {
    eventNames = eventNames.split(/ +/);
  }

  const removeAllAndCallCallback = function(event) {
    eventNames.forEach(function(eventName) {
      target.removeEventListener(eventName, removeAllAndCallCallback, false);
    });
    return callback(event);
  };

  eventNames.forEach(function(eventName) {
    target.addEventListener(eventName, removeAllAndCallCallback, false);
  });
}

/**
 * @param {!Event} e
 * @return {boolean} Whether a modifier key was down when processing |e|.
 */
export function hasKeyModifiers(e) {
  return !!(e.altKey || e.ctrlKey || e.metaKey || e.shiftKey);
}

/**
 * @param {!Element} el
 * @return {boolean} Whether the element is interactive via text input.
 */
export function isTextInputElement(el) {
  return el.tagName === 'INPUT' || el.tagName === 'TEXTAREA';
}

