// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {NearbyShareStates, StatusCode} from './types.js';

/**
 * JavaScript hooks into the native WebUI handler to pass information to the
 * UI Trigger tab.
 */
export class NearbyUiTriggerBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('initializeUiTrigger');
  }

  /**
   * Invokes the NearbyShare service's SendText() method for the input
   * ShareTarget |id|
   * @param {string} id
   * @return {!Promise<!StatusCode>}
   */
  sendText(id) {
    return sendWithPromise('sendText', id);
  }

  /**
   * Invokes the NearbyShare service's Cancel() method for the input
   * ShareTarget |id|
   * @param {string} id
   */
  cancel(id) {
    chrome.send('cancel', [id]);
  }

  /**
   * Invokes the NearbyShare service's Accept() method for the input
   * ShareTarget |id|
   * @param {string} id
   */
  accept(id) {
    chrome.send('accept', [id]);
  }

  /**
   * Invokes the NearbyShare service's Reject() method for the input
   * ShareTarget |id|
   * @param {string} id
   */
  reject(id) {
    chrome.send('reject', [id]);
  }

  /**
   * Invokes the NearbyShare service's Open() method for the input
   * ShareTarget |id|
   * @param {string} id
   */
  open(id) {
    chrome.send('open', [id]);
  }

  /**
   * Registers the UI trigger handler instance as a foreground send surface.
   * @return {!Promise<!StatusCode>}
   */
  registerSendSurfaceForeground() {
    return sendWithPromise('registerSendSurfaceForeground');
  }

  /**
   * Registers the UI trigger handler instance as a background send surface.
   * @return {!Promise<!StatusCode>}
   */
  registerSendSurfaceBackground() {
    return sendWithPromise('registerSendSurfaceBackground');
  }

  /**
   * Unregisters the send surface UI trigger handler instance.
   * @return {!Promise<!StatusCode>}
   */
  unregisterSendSurface() {
    return sendWithPromise('unregisterSendSurface');
  }

  /**
   * Registers the UI trigger handler instance as a foreground receive surface.
   * @return {!Promise<!StatusCode>}
   */
  registerReceiveSurfaceForeground() {
    return sendWithPromise('registerReceiveSurfaceForeground');
  }

  /**
   * Registers the UI trigger handler instance as a background receive surface.
   * @return {!Promise<!StatusCode>}
   */
  registerReceiveSurfaceBackground() {
    return sendWithPromise('registerReceiveSurfaceBackground');
  }

  /**
   * Unregisters the receive surface UI trigger handler instance.
   * @return {!Promise<!StatusCode>}
   */
  unregisterReceiveSurface() {
    return sendWithPromise('unregisterReceiveSurface');
  }

  /**
   * Requests states of Nearby Share booleans.
   * @return {!Promise<!NearbyShareStates>}
   */
  getState() {
    return sendWithPromise('getStates');
  }
}

addSingletonGetter(NearbyUiTriggerBrowserProxy);
