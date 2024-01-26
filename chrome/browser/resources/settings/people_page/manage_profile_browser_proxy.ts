// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage Profile" subpage of
 * the People section to interact with the browser. Chrome Browser only.
 */

// clang-format off
import type {AvatarIcon} from 'chrome://resources/cr_elements/cr_profile_avatar_selector/cr_profile_avatar_selector.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * Contains the possible profile shortcut statuses. These strings must be kept
 * in sync with the C++ Manage Profile handler.
 */
export enum ProfileShortcutStatus {
  PROFILE_SHORTCUT_SETTING_HIDDEN = 'profileShortcutSettingHidden',
  PROFILE_SHORTCUT_NOT_FOUND = 'profileShortcutNotFound',
  PROFILE_SHORTCUT_FOUND = 'profileShortcutFound',
}

export interface ManageProfileBrowserProxy {
  /**
   * Gets the available profile icons to choose from.
   */
  getAvailableIcons(): Promise<AvatarIcon[]>;

  /**
   * Sets the profile's icon to the GAIA avatar.
   */
  setProfileIconToGaiaAvatar(): void;

  /**
   * Sets the profile's icon to one of the default avatars.
   * @param index The new profile avatar index.
   */
  setProfileIconToDefaultAvatar(index: number): void;

  /**
   * Sets the profile's name.
   */
  setProfileName(name: string): void;

  /**
   * Returns whether the current profile has a shortcut.
   */
  getProfileShortcutStatus(): Promise<ProfileShortcutStatus>;

  /**
   * Adds a shortcut for the current profile.
   */
  addProfileShortcut(): void;

  /**
   * Removes the shortcut of the current profile.
   */
  removeProfileShortcut(): void;
}

export class ManageProfileBrowserProxyImpl implements
    ManageProfileBrowserProxy {
  getAvailableIcons() {
    return sendWithPromise('getAvailableIcons');
  }

  setProfileIconToGaiaAvatar() {
    chrome.send('setProfileIconToGaiaAvatar');
  }

  setProfileIconToDefaultAvatar(index: number) {
    chrome.send('setProfileIconToDefaultAvatar', [index]);
  }

  setProfileName(name: string) {
    chrome.send('setProfileName', [name]);
  }

  getProfileShortcutStatus() {
    return sendWithPromise('requestProfileShortcutStatus');
  }

  addProfileShortcut() {
    chrome.send('addProfileShortcut');
  }

  removeProfileShortcut() {
    chrome.send('removeProfileShortcut');
  }

  static getInstance(): ManageProfileBrowserProxy {
    return instance || (instance = new ManageProfileBrowserProxyImpl());
  }

  static setInstance(obj: ManageProfileBrowserProxy) {
    instance = obj;
  }
}

let instance: ManageProfileBrowserProxy|null = null;
