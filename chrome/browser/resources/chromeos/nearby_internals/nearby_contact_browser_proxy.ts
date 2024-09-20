// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * JavaScript hooks into the native WebUI handler to pass Contacts to the
 * Contacts tab.
 */
export class NearbyContactBrowserProxy {
  /** Initializes web contents in the WebUI handler. */
  initialize() {
    chrome.send('initializeContacts');
  }

  /** Downloads the user's contact list from the Nearby Share server. */
  downloadContacts() {
    chrome.send('downloadContacts');
  }

  static getInstance(): NearbyContactBrowserProxy {
    return instance || (instance = new NearbyContactBrowserProxy());
  }
}

let instance: NearbyContactBrowserProxy|null = null;
