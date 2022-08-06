// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="is_chromeos">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {PasswordManagerImpl} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

export const PasswordRequestorMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PasswordRequestorMixinInterface> => {
      class PasswordRequestorMixin extends superClass {
        // <if expr="is_chromeos">
        static get properties() {
          return {tokenRequestManager: Object};
        }

        tokenRequestManager: BlockingRequestManager;
        // </if>

        requestPlaintextPassword(
            id: number,
            reason: chrome.passwordsPrivate.PlaintextReason): Promise<string> {
          // <if expr="is_chromeos">
          // If no password was found, refresh auth token and retry.
          return new Promise(resolve => {
            PasswordManagerImpl.getInstance()
                .requestPlaintextPassword(id, reason)
                .then(password => resolve(password), () => {
                  this.tokenRequestManager.request(() => {
                    this.requestPlaintextPassword(id, reason).then(resolve);
                  });
                });
          });
          // </if>
          // <if expr="not (chromeos_ash or chromeos_lacros)">
          return PasswordManagerImpl.getInstance().requestPlaintextPassword(
              id, reason);
          // </if>
        }
      }

      return PasswordRequestorMixin;
    });

export interface PasswordRequestorMixinInterface {
  requestPlaintextPassword(
      id: number,
      reason: chrome.passwordsPrivate.PlaintextReason): Promise<string>;
  // <if expr="is_chromeos">
  tokenRequestManager: BlockingRequestManager;
  // </if>
}
