// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface KeepAlive {
  origin: string;
  count: number;
}

export interface ProfileState {
  foregroundColor: string;
  backgroundColor: string;
  profilePath: string;
  localProfileName: string;
  signinState: string;
  signinRequired: boolean;
  gaiaName: string;
  gaiaId: string;
  userName: string;
  hostedDomain: string;
  isSupervised: boolean;
  isOmitted: boolean;
  isEphemeral: boolean;
  userAcceptedAccountManagement: boolean;
  keepAlives: KeepAlive[];
  signedAccounts: string[];
  isLoaded: boolean;
  hasOffTheRecord: boolean;
}

export interface ProfileStateElement {
  className: string;
  profileState: ProfileState;
  expanded: boolean;
}

/**
 * @fileoverview A helper object used by the profile internals debug page
 * to interact with the browser.
 */
export interface ProfileInternalsBrowserProxy {
  getProfilesList(): void;
}

export class ProfileInternalsBrowserProxyImpl implements
    ProfileInternalsBrowserProxy {
  getProfilesList(): void {
    chrome.send('getProfilesList');
  }

  static getInstance(): ProfileInternalsBrowserProxy {
    return instance || (instance = new ProfileInternalsBrowserProxyImpl());
  }

  static setInstance(obj: ProfileInternalsBrowserProxy) {
    instance = obj;
  }
}

let instance: ProfileInternalsBrowserProxy|null = null;
