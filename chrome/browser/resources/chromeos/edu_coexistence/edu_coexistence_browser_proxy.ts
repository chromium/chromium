// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AuthCompletedCredentials} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

import {EduCoexistenceParams} from './edu_coexistence_controller.js';

export interface EduCoexistenceBrowserProxy {
  /** Sends 'initialize' message to prepare for starting auth. */
  initializeLogin(): void;

  /**
   * Sends 'initializeEduArgs' message to provide the parameters.
   */
  initializeEduArgs(): Promise<EduCoexistenceParams>;

  /**
   * Sends 'authenticatorReady' message to handle tasks after authenticator
   * loads.
   */
  authenticatorReady(): void;

  /**
   * Sends 'completeLogin' message to complete login.
   */
  completeLogin(credentials: AuthCompletedCredentials): void;

  /**
   * Sends 'getAccounts' message to the handler. The promise will be resolved
   * with the list of emails of accounts in session.
   */
  getAccounts(): Promise<string[]>;

  /**
   * Sends 'getDeviceId' message to the handler. The promise will be resolved
   * with the device identifier for this user.
   */
  getDeviceId(): Promise<string>;

  /**
   * Sends 'consentValid' message to the handler to notify the handler that
   * the parental consent is valid.
   */
  consentValid(): void;

  /**
   * Sends 'consentLogged' message to the handler to notify the handler that
   * the parental consent is valid. Returns a promise which will resolve to true
   * when the account has successfully been added. The promise will be used
   * by the server flow to show "Account added" page.
   */
  consentLogged(account: string, eduCoexistenceToSVersion: string):
      Promise<boolean>;

  /** Sends 'dialogClose' message to close the login dialog. */
  dialogClose(): void;

  /**
   * Sends 'error' message to handler.
   */
  onError(msg: string[]): void;

  /**
   * Returns JSON-encoded dialog arguments.
   */
  getDialogArguments(): string;
}

export class EduCoexistenceBrowserProxyImpl implements
    EduCoexistenceBrowserProxy {
  initializeLogin() {
    chrome.send('initialize');
  }

  initializeEduArgs() {
    return sendWithPromise('initializeEduArgs');
  }

  authenticatorReady() {
    chrome.send('authenticatorReady');
  }

  completeLogin(credentials: AuthCompletedCredentials) {
    chrome.send('completeLogin', [credentials]);
  }

  getAccounts() {
    return sendWithPromise('getAccounts');
  }

  getDeviceId() {
    return sendWithPromise('getDeviceId');
  }

  consentValid() {
    chrome.send('consentValid');
  }

  consentLogged(account: string, eduCoexistenceToSVersion: string) {
    return sendWithPromise(
        'consentLogged', [account, eduCoexistenceToSVersion]);
  }

  dialogClose() {
    chrome.send('dialogClose');
  }

  onError(msg: string[]) {
    chrome.send('error', msg);
  }

  getDialogArguments() {
    return chrome.getVariableValue('dialogArguments');
  }

  static getInstance(): EduCoexistenceBrowserProxy {
    return instance || (instance = new EduCoexistenceBrowserProxyImpl());
  }

  static setInstance(obj: EduCoexistenceBrowserProxy) {
    instance = obj;
  }
}

let instance: EduCoexistenceBrowserProxy|null = null;
