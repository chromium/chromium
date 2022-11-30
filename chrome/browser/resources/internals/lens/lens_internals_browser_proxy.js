// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class LensInternalsBrowserProxy {
  /**
   * Start debug mode collection for proactive.
   * @return {!Promise<void>} A promise firing when the call is complete.
   */
  startDebugMode() {}

  /**
   * Start debug mode collection for proactive.
   * @return {!Promise<!Array<!Array<string>>>} A promise firing when the call
   *     is complete.
   */
  refreshDebugData() {}

  /**
   * Stop debug mode collection for proactive.
   * @return {!Promise<void>} A promise firing when the call is complete.
   */
  stopDebugMode() {}
}

/**
 * @implements {LensInternalsBrowserProxy}
 */
export class LensInternalsBrowserProxyImpl {
  /** @override */
  startDebugMode() {
    return sendWithPromise('startDebugMode');
  }

  /** @override */
  refreshDebugData() {
    return sendWithPromise('refreshDebugData');
  }

  /** @override */
  stopDebugMode() {
    return sendWithPromise('stopDebugMode');
  }
}

addSingletonGetter(LensInternalsBrowserProxyImpl);
