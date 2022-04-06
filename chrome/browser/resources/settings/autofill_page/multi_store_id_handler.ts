// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common code used by MultiStorePasswordUiEntry and
 * MultiStoreIdHandler to deal with ids from different stores.
 */

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';

export class MultiStoreIdHandler {
  private deviceId_: number|null = null;
  private accountId_: number|null = null;

  /**
   * Get any of the present ids. At least one of the ids must have been set
   * before this method is invoked.
   */
  getAnyId(): number {
    if (this.deviceId_ !== null) {
      return this.deviceId_;
    }
    if (this.accountId_ !== null) {
      return this.accountId_;
    }
    assertNotReached();
  }

  /**
   * @return Whether one of the ids is from the account.
   */
  isPresentInAccount(): boolean {
    return this.accountId_ !== null;
  }

  /**
   * @return Whether one of the ids is from the device.
   */
  isPresentOnDevice(): boolean {
    return this.deviceId_ !== null;
  }

  get deviceId(): number|null {
    return this.deviceId_;
  }
  get accountId(): number|null {
    return this.accountId_;
  }

  protected setId(id: number, fromAccountStore: boolean) {
    if (fromAccountStore) {
      this.accountId_ = id;
    } else {
      this.deviceId_ = id;
    }
  }
}
