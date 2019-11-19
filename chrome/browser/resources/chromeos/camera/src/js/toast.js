// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for toast.
 */
cca.toast = cca.toast || {};

/**
 * Shows a toast message.
 * @param {string} message Message to be shown.
 */
cca.toast.show = function(message) {
  cca.toast.update_(message, false);
};

/**
 * Speaks a toast message.
 * @param {string} message Message to be spoken.
 */
cca.toast.speak = function(message) {
  cca.toast.update_(message, true);
};

/**
 * Updates the toast message.
 * @param {string} message Message to be updated.
 * @param {boolean} spoken Whether the toast is spoken only.
 * @private
 */
cca.toast.update_ = function(message, spoken) {
  // TTS speaks changes of on-screen aria-live elements. Force content changes
  // and clear content once inactive to avoid stale content being read out.
  var element = /** @type {!HTMLElement} */ (document.querySelector('#toast'));
  cca.util.animateCancel(element); // Cancel the active toast if any.
  element.textContent = ''; // Force to reiterate repeated messages.
  element.textContent = chrome.i18n.getMessage(message) || message;

  element.classList.toggle('spoken', spoken);
  cca.util.animateOnce(element, () => element.textContent = '');
};
