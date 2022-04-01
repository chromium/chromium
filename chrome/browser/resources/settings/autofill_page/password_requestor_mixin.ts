// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {PasswordManagerImpl} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

export const PasswordRequestorMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PasswordRequestorMixinInterface> => {
      class PasswordRequestorMixin extends superClass {
        // <if expr="chromeos_ash or chromeos_lacros">
        static get properties() {
          return {tokenRequestManager: Object};
        }

        tokenRequestManager: BlockingRequestManager;
        // </if>

        requestPlaintextPassword(
            id: number,
            reason: chrome.passwordsPrivate.PlaintextReason): Promise<string> {
          return new Promise(resolve => {
            PasswordManagerImpl.getInstance()
                .requestPlaintextPassword(id, reason)
                .then(password => resolve(password), () => {
                  // <if expr="chromeos_ash or chromeos_lacros">
                  // If no password was found, refresh auth token and retry.
                  this.tokenRequestManager.request(() => {
                    this.requestPlaintextPassword(id, reason).then(resolve);
                  });
                  // </if>
                });
          });
        }

        getPlaintextInsecurePassword(
            credential: chrome.passwordsPrivate.InsecureCredential,
            reason: chrome.passwordsPrivate.PlaintextReason):
            Promise<chrome.passwordsPrivate.InsecureCredential> {
          return new Promise(resolve => {
            PasswordManagerImpl.getInstance()
                .getPlaintextInsecurePassword(credential, reason)
                .then(insecureCredential => resolve(insecureCredential), () => {
                  // <if expr="chromeos_ash or chromeos_lacros">
                  // If no password was found, refresh auth token and retry.
                  this.tokenRequestManager.request(() => {
                    this.getPlaintextInsecurePassword(credential, reason)
                        .then(resolve);
                  });
                  // </if>
                });
          });
        }
      }

      return PasswordRequestorMixin;
    });

export interface PasswordRequestorMixinInterface {
  requestPlaintextPassword(
      id: number,
      reason: chrome.passwordsPrivate.PlaintextReason): Promise<string>;
  getPlaintextInsecurePassword(
      credential: chrome.passwordsPrivate.InsecureCredential,
      reason: chrome.passwordsPrivate.PlaintextReason):
      Promise<chrome.passwordsPrivate.InsecureCredential>;
  // <if expr="chromeos_ash or chromeos_lacros">
  tokenRequestManager: BlockingRequestManager;
  // </if>
}
