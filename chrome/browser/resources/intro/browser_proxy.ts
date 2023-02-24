// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the chrome://intro page to
 * interact with the browser.
 */

// <if expr="chromeos_lacros">
// Profile info sent from C++.
export interface LacrosIntroProfileInfo {
  pictureUrl: string;
  title: string;
  subtitle: string;
  managementDisclaimer: string;
}
// </if>

export interface IntroBrowserProxy {
  // Called when the user clicks the "sign in" button.
  continueWithAccount(): void;

  // <if expr="enable_dice_support">
  // Called when the user clicks the "continue without account" button.
  continueWithoutAccount(): void;
  // </if>

  // Initializes the FRE intro main view.
  initializeMainView(): void;
}

export class IntroBrowserProxyImpl implements IntroBrowserProxy {
  continueWithAccount() {
    chrome.send('continueWithAccount');
  }

  // <if expr="enable_dice_support">
  continueWithoutAccount() {
    chrome.send('continueWithoutAccount');
  }
  //</if>

  initializeMainView() {
    chrome.send('initializeMainView');
  }

  static getInstance(): IntroBrowserProxy {
    return instance || (instance = new IntroBrowserProxyImpl());
  }

  static setInstance(obj: IntroBrowserProxy) {
    instance = obj;
  }
}

let instance: IntroBrowserProxy|null = null;
