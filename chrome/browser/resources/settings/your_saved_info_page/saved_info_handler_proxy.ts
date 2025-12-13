// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * @fileoverview A helper object used by the "Your saved info" page
 * to interact with the browser to obtain data types counters
 */

export interface PasswordCount {
  passwordCount: number;
  passkeyCount: number;
}

export interface SavedInfoHandlerProxy {
  /**
   * Get the number of passwords and passkeys.
   */
  getPasswordCount(): Promise<PasswordCount>;

  /**
   * Get the number of loyalty cards.
   */
  getLoyaltyCardsCount(): Promise<number|undefined>;
}

export class SavedInfoHandlerImpl implements SavedInfoHandlerProxy {
  getPasswordCount() {
    return sendWithPromise('getPasswordCount');
  }

  getLoyaltyCardsCount() {
    return sendWithPromise('getLoyaltyCardsCount');
  }

  static getInstance(): SavedInfoHandlerProxy {
    return instance || (instance = new SavedInfoHandlerImpl());
  }

  static setInstance(obj: SavedInfoHandlerProxy) {
    instance = obj;
  }
}

let instance: SavedInfoHandlerProxy|null = null;
