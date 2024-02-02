// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the profilecustomization bubble to
 * interact with the browser.
 */

import type {AvatarIcon} from 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

// Profile info (colors and avatar) sent from C++.
export interface ProfileInfo {
  backgroundColor: string;
  pictureUrl: string;
  isManaged: boolean;
  welcomeTitle: string;
}

export interface ProfileCustomizationBrowserProxy {
  // Called when the page is ready.
  initialized(): Promise<ProfileInfo>;

  // Retrieves custom avatar list for the select avatar dialog.
  getAvailableIcons(): Promise<AvatarIcon[]>;

  // Called when the user clicks the done button.
  done(profileName: string): void;

  // Called when the user clicks the skip button.
  skip(): void;

  // Called when the user clicks the delete profile button.
  deleteProfile(): void;

  setAvatarIcon(avatarIndex: number): void;
}

export class ProfileCustomizationBrowserProxyImpl implements
    ProfileCustomizationBrowserProxy {
  initialized() {
    return sendWithPromise('initialized');
  }

  getAvailableIcons() {
    return sendWithPromise('getAvailableIcons');
  }

  done(profileName: string) {
    chrome.send('done', [profileName]);
  }

  skip() {
    chrome.send('skip');
  }

  deleteProfile() {
    chrome.send('deleteProfile');
  }

  setAvatarIcon(avatarIndex: number) {
    chrome.send('setAvatarIcon', [avatarIndex]);
  }

  static getInstance(): ProfileCustomizationBrowserProxy {
    return instance || (instance = new ProfileCustomizationBrowserProxyImpl());
  }

  static setInstance(obj: ProfileCustomizationBrowserProxy) {
    instance = obj;
  }
}

let instance: ProfileCustomizationBrowserProxy|null = null;
