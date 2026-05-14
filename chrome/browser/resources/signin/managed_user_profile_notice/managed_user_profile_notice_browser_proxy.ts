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

// LINT.IfChange(ScreenType)
export enum ScreenType {
  ENTERPRISE_ACCOUNT_SYNC_ENABLED = 0,
  ENTERPRISE_ACCOUNT_SYNC_DISABLED = 1,
  CONSUMER_ACCOUNT_SYNC_DISABLED = 2,
  ENTERPRISE_ACCOUNT_CREATION = 3,
  ENTERPRISE_OIDC = 4,
  PROFILE_PICKER = 5,
  FIRST_RUN = 6,
}
// LINT.ThenChange(//chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h:ScreenType)

export enum AppMode {
  FIRST_RUN = 'first-run',
  PROFILE_PICKER = 'profile-picker',
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

  matchMedia(query: string): MediaQueryList;
}

export class ManagedUserProfileNoticeBrowserProxyImpl implements
  ManagedUserProfileNoticeBrowserProxy {
  initialized() {
    return sendWithPromise<ManagedUserProfileInfo>('initialized');
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

  matchMedia(query: string): MediaQueryList {
    return window.matchMedia(query);
  }
}

let instance: ManagedUserProfileNoticeBrowserProxy|null = null;
