// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

import {NearbyShareStates, StatusCode} from './types.js';

export class NearbyPresenceBrowserProxy {
  /**
   * Initializes web contents in the WebUI handler.
   */
  initialize() {
    chrome.send('initializePresenceHandler');
  }

  /**
   * Send StartPresenceScan which will be received by the
   * NearbyInternalsPresenceHandler.
   */
  SendStartScan() {
    chrome.send('StartPresenceScan');
  }
}

addSingletonGetter(NearbyPresenceBrowserProxy);
