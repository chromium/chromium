// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

let instance: LensInternalsBrowserProxy|null = null;

export interface LensInternalsBrowserProxy {
  /**
   * Start debug mode collection for proactive.
   * @return A promise firing when the call is complete.
   */
  startDebugMode(): Promise<void>;

  /**
   * Start debug mode collection for proactive.
   * @return  A promise firing when the call is complete.
   */
  refreshDebugData(): Promise<string[][]>;

  /**
   * Stop debug mode collection for proactive.
   * @return A promise firing when the call is complete.
   */
  stopDebugMode(): Promise<void>;
}

export class LensInternalsBrowserProxyImpl implements
    LensInternalsBrowserProxy {
  startDebugMode() {
    return sendWithPromise('startDebugMode');
  }

  refreshDebugData() {
    return sendWithPromise('refreshDebugData');
  }

  stopDebugMode() {
    return sendWithPromise('stopDebugMode');
  }

  static getInstance(): LensInternalsBrowserProxy {
    return instance || (instance = new LensInternalsBrowserProxyImpl());
  }
}
