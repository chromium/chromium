// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface Passkey {
  // credentialId is hex-encoded.
  credentialId: string;
  relyingPartyId: string;
  userName: string;
  userDisplayName: string;
}

export interface PasskeysBrowserProxy {
  /**
   * Determines whether any passkeys exist on the local device. Should be
   * significantly more efficient than calling `enumerate` and checking for a
   * non-empty list, but may report false positives if the last passkey was
   * recently deleted.
   */
  hasPasskeys(): Promise<boolean>;

  /**
   * Enumerates passkeys from the local device. Result is null if management
   * is not supported on this platform.
   */
  enumerate(): Promise<Passkey[]|null>;

  /**
   * Deletes a passkey as specified by credentialId then performs an `enumerate`
   * operation.
   */
  delete(credentialId: string): Promise<Passkey[]|null>;

  /**
   * Edits a passkey's credential metadata username then performs an `enumerate`
   * operation.
   */
  edit(credentialId: string, newUsername: string): Promise<Passkey[]|null>;
}

export class PasskeysBrowserProxyImpl implements PasskeysBrowserProxy {
  hasPasskeys() {
    return sendWithPromise('passkeysHasPasskeys');
  }

  enumerate() {
    return sendWithPromise('passkeysEnumerate');
  }

  delete(credentialId: string) {
    return sendWithPromise('passkeysDelete', credentialId);
  }

  edit(credentialId: string, newUsername: string) {
    return sendWithPromise('passkeysEdit', credentialId, newUsername);
  }

  static getInstance(): PasskeysBrowserProxy {
    return passkeysProxyInstance ||
        (passkeysProxyInstance = new PasskeysBrowserProxyImpl());
  }

  static setInstance(obj: PasskeysBrowserProxy) {
    passkeysProxyInstance = obj;
  }
}

let passkeysProxyInstance: PasskeysBrowserProxy|null = null;
