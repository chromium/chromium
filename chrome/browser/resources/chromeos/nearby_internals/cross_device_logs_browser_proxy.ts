// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {LogMessage} from './types.js';

/**
 * JavaScript hooks into the native WebUI handler to pass LogMessages to the
 * logging tab.
 */
export class NearbyLogsBrowserProxy {
  getLogMessages(): Promise<LogMessage[]> {
    return sendWithPromise('getLogMessages');
  }

  getQuickPairLogMessages(): Promise<LogMessage[]> {
    return sendWithPromise('getQuickPairLogMessages');
  }

  static getInstance(): NearbyLogsBrowserProxy {
    return instance || (instance = new NearbyLogsBrowserProxy());
  }
}

let instance: NearbyLogsBrowserProxy|null = null;
