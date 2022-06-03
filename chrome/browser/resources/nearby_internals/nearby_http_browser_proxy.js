// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {HttpMessage} from './types.js';

/**
 * JavaScript hooks into the native WebUI handler to pass HttpMessages to the
 * Http Messages tab.
 */
export class NearbyHttpBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('initializeHttp');
  }

  /**
   * Triggers UpdateDevice RPC.
   */
  updateDevice() {
    chrome.send('updateDevice');
  }

  /**
   * Triggers ListContactPeople RPC.
   */
  listContactPeople() {
    chrome.send('listContactPeople');
  }

  /**
   * Triggers ListPublicCertificates RPC.
   */
  listPublicCertificates() {
    chrome.send('listPublicCertificates');
  }
}

addSingletonGetter(NearbyHttpBrowserProxy);
