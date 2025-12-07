// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

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
  sendText(id: string) {
    return sendWithPromise('sendText', id);
  }

  /**
   * Invokes the NearbyShare service's Cancel() method for the input
   * ShareTarget |id|
   * @param {string} id
   */
  cancel(id: string) {
    chrome.send('cancel', [id]);
  }

  /**
   * Invokes the NearbyShare service's Accept() method for the input
   * ShareTarget |id|
   * @param {string} id
   */
  accept(id: string) {
    chrome.send('accept', [id]);
  }

  /**
   * Invokes the NearbyShare service's Reject() method for the input
   * ShareTarget |id|
   * @param {string} id
   */
  reject(id: string) {
    chrome.send('reject', [id]);
  }

  /**
   * Invokes the NearbyShare service's Open() method for the input
   * ShareTarget |id|
   * @param {string} id
   */
  open(id: string) {
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

  /**
   * Tells C++ side to trigger a Nearby Share received notification.
   */
  showNearbyShareReceivedNotification() {
    chrome.send('showNearbyShareReceivedNotification');
  }

  /**
   * Tells C++ side to trigger a Fast Pair error notification.
   */
  notifyFastPairError() {
    chrome.send('notifyFastPairError');
  }

  /**
   * Tells C++ side to trigger a Fast Pair discovery notification.
   */
  notifyFastPairDiscovery() {
    chrome.send('notifyFastPairDiscovery');
  }

  /**
   * Tells C++ side to trigger a Fast Pair pairing notification.
   */
  notifyFastPairPairing() {
    chrome.send('notifyFastPairPairing');
  }

  /**
   * Tells C++ side to trigger a Fast Pair application available notification.
   */
  notifyFastPairApplicationAvailable() {
    chrome.send('notifyFastPairApplicationAvailable');
  }

  /**
   * Tells C++ side to trigger a Fast Pair application installed notification.
   */
  notifyFastPairApplicationInstalled() {
    chrome.send('notifyFastPairApplicationInstalled');
  }

  /**
   * Tells C++ side to trigger a Fast Pair associate account notification.
   */
  notifyFastPairAssociateAccount() {
    chrome.send('notifyFastPairAssociateAccount');
  }

  static getInstance(): NearbyUiTriggerBrowserProxy {
    return instance || (instance = new NearbyUiTriggerBrowserProxy());
  }
}

let instance: NearbyUiTriggerBrowserProxy|null = null;
