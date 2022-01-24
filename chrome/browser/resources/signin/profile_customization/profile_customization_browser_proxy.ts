// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the profilecustomization bubble to
 * interact with the browser.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

// Profile info (colors and avatar) sent from C++.
export type ProfileInfo = {
  backgroundColor: string,
  pictureUrl: string,
  isManaged: boolean,
  welcomeTitle: string,
};

export interface ProfileCustomizationBrowserProxy {
  // Called when the page is ready.
  initialized(): Promise<ProfileInfo>;

  // Called when the user clicks the done button.
  done(profileName: string): void;
}

export class ProfileCustomizationBrowserProxyImpl implements
    ProfileCustomizationBrowserProxy {
  initialized() {
    return sendWithPromise('initialized');
  }

  done(profileName: string) {
    chrome.send('done', [profileName]);
  }

  static getInstance(): ProfileCustomizationBrowserProxy {
    return instance || (instance = new ProfileCustomizationBrowserProxyImpl());
  }

  static setInstance(obj: ProfileCustomizationBrowserProxy) {
    instance = obj;
  }
}

let instance: ProfileCustomizationBrowserProxy|null = null;
