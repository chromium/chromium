// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the managed user profile notice screen
 * to interact with the browser.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export enum BrowsingDataHandling {
  MERGE = 'merge',
  SEPARATE = 'separate',
}

export enum State {
  DISCLOSURE = 0,
  PROCESSING = 1,
  SUCCESS = 2,
  TIMEOUT = 3,
  ERROR = 4,
  VALUE_PROPOSITION = 5,
  USER_DATA_HANDLING = 6,
}

// Managed user profile info sent from C++.
export interface ManagedUserProfileInfo {
  accountName: string;
  continueAs: string;
  email: string;
  pictureUrl: string;
  showEnterpriseBadge: boolean;
  title: string;
  subtitle: string;
  enterpriseInfo: string;
  proceedLabel: string;
  checkLinkDataCheckboxByDefault: boolean;
}

export interface ManagedUserProfileNoticeBrowserProxy {
  // Called when the page is ready
  initialized(): Promise<ManagedUserProfileInfo>;

  initializedWithSize(height: number): void;

  /**
   * Called when the user clicks the proceed button.
   */
  proceed(state: State, linkData: boolean): void;

  /**
   * Called when the user clicks the cancel button.
   */
  cancel(): void;
}

export class ManagedUserProfileNoticeBrowserProxyImpl implements
  ManagedUserProfileNoticeBrowserProxy {
  initialized() {
    return sendWithPromise('initialized');
  }

  initializedWithSize(height: number) {
    chrome.send('initializedWithSize', [height]);
  }

  proceed(state: State, linkData: boolean) {
    chrome.send('proceed', [state, linkData]);
  }

  cancel() {
    chrome.send('cancel');
  }

  static getInstance(): ManagedUserProfileNoticeBrowserProxy {
    return instance ||
        (instance = new ManagedUserProfileNoticeBrowserProxyImpl());
  }

  static setInstance(obj: ManagedUserProfileNoticeBrowserProxy) {
    instance = obj;
  }
}

let instance: ManagedUserProfileNoticeBrowserProxy|null = null;
