// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a wrapper around cr.ts as a workaround for the closure compiler.
// Do not use in TypeScript code, use ui/webui/resources/js/cr.ts directly.

import {addWebUiListener as crAddWebUiListener, removeWebUiListener as crRemoveWebUiListener, sendWithPromise as crSendWithPromise, webUIListenerCallback as crWebUIListenerCallback, webUIResponse as crWebUIResponse} from '//resources/js/cr.js';

/** @typedef {{eventName: string, uid: number}} */
export let WebUIListener;

/**
 * The named method the WebUI handler calls directly in response to a
 * chrome.send call that expects a response. The handler requires no knowledge
 * of the specific name of this method, as the name is passed to the handler
 * as the first argument in the arguments list of chrome.send. The handler
 * must pass the ID, also sent via the chrome.send arguments list, as the
 * first argument of the JS invocation; additionally, the handler may
 * supply any number of other arguments that will be included in the response.
 * @param {string} id The unique ID identifying the Promise this response is
 *     tied to.
 * @param {boolean} isSuccess Whether the request was successful.
 * @param {*} response The response as sent from C++.
 */
export function webUIResponse(id, isSuccess, response) {
  crWebUIResponse(id, isSuccess, response);
}

/**
 * A variation of chrome.send, suitable for messages that expect a single
 * response from C++.
 * @param {string} methodName The name of the WebUI handler API.
 * @param {...*} var_args Variable number of arguments to be forwarded to the
 *     C++ call.
 * @return {!Promise}
 */
export function sendWithPromise(methodName, var_args) {
  const args = Array.prototype.slice.call(arguments, 1);
  return crSendWithPromise(methodName, ...args);
}

/**
 * The named method the WebUI handler calls directly when an event occurs.
 * The WebUI handler must supply the name of the event as the first argument
 * of the JS invocation; additionally, the handler may supply any number of
 * other arguments that will be forwarded to the listener callbacks.
 * @param {string} event The name of the event that has occurred.
 * @param {...*} var_args Additional arguments passed from C++.
 */
export function webUIListenerCallback(event, var_args) {
  const args = Array.prototype.slice.call(arguments, 1);
  crWebUIListenerCallback(event, ...args);
}

/**
 * Registers a listener for an event fired from WebUI handlers. Any number of
 * listeners may register for a single event.
 * @param {string} eventName The event to listen to.
 * @param {!Function} callback The callback run when the event is fired.
 * @return {!WebUIListener} An object to be used for removing a listener via
 *     removeWebUIListener. Should be treated as read-only.
 */
export function addWebUIListener(eventName, callback) {
  return crAddWebUiListener(eventName, callback);
}

/**
 * Removes a listener. Does nothing if the specified listener is not found.
 * @param {!WebUIListener} listener The listener to be removed (as returned by
 *     addWebUIListener).
 * @return {boolean} Whether the given listener was found and actually
 *     removed.
 */
export function removeWebUIListener(listener) {
  return crRemoveWebUiListener(listener);
}
