// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper object and related behavior that encapsulate messaging
 * between JS and C++ for creating/importing profiles in the user-manager page.
 */

/**
 * @typedef {{name: string,
 *            filePath: string}}
 */
let ProfileInfo;

cr.define('signin', function() {
  /** @interface */
  class ProfileBrowserProxy {
    /**
     * Gets the available profile icons to choose from.
     */
    getAvailableIcons() {}

    /**
     * Launches the guest user.
     */
    launchGuestUser() {}

    /**
     * Creates a profile.
     * @param {string} profileName Name of the new profile.
     * @param {string} profileIconUrl URL of the selected icon of the new
     *     profile.
     * @param {boolean} createShortcut if true a desktop shortcut will be
     *     created.
     */
    createProfile(profileName, profileIconUrl, createShortcut) {}

    /**
     * Initializes the UserManager
     * @param {string} locationHash
     */
    initializeUserManager(locationHash) {}

    /**
     * Launches the user with the given |profilePath|
     * @param {string} profilePath Profile Path of the user.
     */
    launchUser(profilePath) {}

    /**
     * Opens the given url in a new tab in the browser instance of the last
     * active profile. Hyperlinks don't work in the user manager since its
     * browser instance does not support tabs.
     * @param {string} url
     */
    openUrlInLastActiveProfileBrowser(url) {}

    /**
     * Switches to the profile with the given path.
     * @param {string} profilePath Path to the profile to switch to.
     */
    switchToProfile(profilePath) {}

    /**
     * @return {!Promise<boolean>} Whether all (non-supervised and non-child)
     *     profiles are locked.
     */
    areAllProfilesLocked() {}

    /**
     * Authenticates the custodian profile with the given email address.
     * @param {string} emailAddress Email address of the custodian profile.
     */
    authenticateCustodian(emailAddress) {}
  }

  /** @implements {signin.ProfileBrowserProxy} */
  class ProfileBrowserProxyImpl {
    /** @override */
    getAvailableIcons() {
      chrome.send('requestDefaultProfileIcons');
    }

    /** @override */
    launchGuestUser() {
      chrome.send('launchGuest');
    }

    /** @override */
    createProfile(profileName, profileIconUrl, createShortcut) {
      chrome.send(
          'createProfile', [profileName, profileIconUrl, createShortcut]);
    }

    /** @override */
    initializeUserManager(locationHash) {
      chrome.send('userManagerInitialize', [locationHash]);
    }

    /** @override */
    launchUser(profilePath) {
      chrome.send('launchUser', [profilePath]);
    }

    /** @override */
    openUrlInLastActiveProfileBrowser(url) {
      chrome.send('openUrlInLastActiveProfileBrowser', [url]);
    }

    /** @override */
    switchToProfile(profilePath) {
      chrome.send('switchToProfile', [profilePath]);
    }

    /** @override */
    areAllProfilesLocked() {
      return cr.sendWithPromise('areAllProfilesLocked');
    }

    /** @override */
    authenticateCustodian(emailAddress) {
      chrome.send('authenticateCustodian', [emailAddress]);
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(ProfileBrowserProxyImpl);

  return {
    ProfileBrowserProxy: ProfileBrowserProxy,
    ProfileBrowserProxyImpl: ProfileBrowserProxyImpl,
  };
});
