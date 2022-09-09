// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for keyboard utility functions.
 */
/* #export */ var keyboard = {};

/**
 * keyboard_utils may be injected as content script. This variable gets and
 * saves the host window.
 * @private
 */
var keyboardHostWindow;

/**
 * keyboard_utils may be injected as content script. This variable gets and
 * saves the host origin.
 * @private
 */
var keyboardHostOrigin;

/**
 * Handles the initial messaging posted from webview, where this script is
 * injected.
 * @param {Event} event Message event posted from webview.
 * @private
 */
keyboard.onInitMessage_ = function(event) {
  if (event.data == 'initialMessage' && event.origin == 'chrome://oobe') {
    keyboardHostWindow = event.source;
    keyboardHostOrigin = event.origin;
  }
};

/**
 * Handles the actual focus advancing by raising tab/shift-tab key events
 * on C++ side.
 * @param {boolean} reverse true if focus is moving backward, otherwise forward.
 */
keyboard.onAdvanceFocus = function(reverse) {
  chrome.send('raiseTabKeyEvent', [reverse]);
};

/**
 * Swallows keypress and keyup events of arrow keys.
 * @param {!Event} event Raised event.
 * @private
 */
keyboard.onKeyIgnore_ = function(event) {
  event = /** @type {!KeyboardEvent} */ (event);

  if (event.ctrlKey || event.shiftKey || event.altKey || event.metaKey) {
    return;
  }

  if (event.key == 'ArrowLeft' || event.key == 'ArrowRight' ||
      event.key == 'ArrowUp' || event.key == 'ArrowDown') {
    event.stopPropagation();
    event.preventDefault();
  }
};

/**
 * Handles arrow key events, depending on if self is a content script.
 * @param {!Event} event Raised event.
 * @private
 */
keyboard.onKeyDown_ = function(event) {
  event = /** @type {!KeyboardEvent} */ (event);

  if (event.ctrlKey || event.shiftKey || event.altKey || event.metaKey) {
    return;
  }

  // This file also gets embedded inside of the CfM/hotrod enrollment webview.
  // Events will bubble down into the webview, which means that the event
  // handler from the webui will steal the events meant for the webview. So we
  // have to disable the webui handler if the active element is the webview.
  //
  // $ is defined differently depending on how this file gets executed; we have
  // to use document.getElementById to get consistent behavior.
  //
  // See crbug.com/543865.
  if (document.activeElement ===
      // eslint-disable-next-line no-restricted-properties
      document.getElementById('authView')) {
    return;
  }

  // The networks list in the Chrome OOBE has an iron-list which uses arrow
  // keys to navigate elements. Tab events will remove focus from the list.
  //
  // $ is defined differently depending on how this file gets executed; we have
  // to use document.getElementById to get consistent behavior.
  //
  // See crbug.com/1083145
  // eslint-disable-next-line no-restricted-properties
  if (document.activeElement === document.getElementById('network-selection') &&
      document.activeElement.shadowRoot.activeElement.tagName ==
      'NETWORK-SELECT-LOGIN' &&
      (event.key == 'ArrowUp' || event.key == 'ArrowDown')) {
    return;
  }

  // Do not map arrow key events to tab events if the user is currently
  // focusing an input element and hits the left or right.
  var needsLeftRightKey =
      (event.key == 'ArrowLeft' || event.key == 'ArrowRight') &&
      document.activeElement.tagName == 'INPUT';

  if (!needsLeftRightKey) {
    // Inside of a content script, instead of callling chrome.send directly
    // (since it is not available) we send an event to the host script
    // which will make the chrome.send call on our behalf.
    if (event.key == 'ArrowLeft' || event.key == 'ArrowUp') {
      if (!keyboardHostWindow) {
        keyboard.onAdvanceFocus(true);
      } else {
        keyboardHostWindow.postMessage('backwardFocus', keyboardHostOrigin);
      }
      event.preventDefault();
    } else if (event.key == 'ArrowRight' || event.key == 'ArrowDown') {
      if (!keyboardHostWindow) {
        keyboard.onAdvanceFocus(false);
      } else {
        keyboardHostWindow.postMessage('forwardFocus', keyboardHostOrigin);
      }
      event.preventDefault();
    }
  }

  if (event.key == 'ArrowLeft' || event.key == 'ArrowUp' ||
      event.key == 'ArrowRight' || event.key == 'ArrowDown') {
    event.stopPropagation();
  }
};

/**
 * Initializes event handling for arrow keys driven focus flow.
 * @param {boolean} injected true if script runs as an injected content script.
 */
keyboard.initializeKeyboardFlow = function(injected) {
  document.addEventListener('keydown', keyboard.onKeyDown_, true);
  document.addEventListener('keypress', keyboard.onKeyIgnore_, true);
  document.addEventListener('keyup', keyboard.onKeyIgnore_, true);
  if (injected) {
    window.addEventListener('message', keyboard.onInitMessage_);
  }
};
