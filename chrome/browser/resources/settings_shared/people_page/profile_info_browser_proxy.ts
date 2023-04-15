// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * An object describing the profile.
 */
export interface ProfileInfo {
  name: string;
  iconUrl: string;
}

export interface ProfileInfoBrowserProxy {
  /**
   * Returns a Promise for the profile info.
   */
  getProfileInfo(): Promise<ProfileInfo>;

  /**
   * Requests the profile stats count. The result is returned by the
   * 'profile-stats-count-ready' WebUI listener event.
   */
  getProfileStatsCount(): void;
}

export class ProfileInfoBrowserProxyImpl implements ProfileInfoBrowserProxy {
  getProfileInfo() {
    return sendWithPromise('getProfileInfo');
  }

  getProfileStatsCount() {
    chrome.send('getProfileStatsCount');
  }

  static getInstance(): ProfileInfoBrowserProxy {
    return instance || (instance = new ProfileInfoBrowserProxyImpl());
  }

  static setInstance(obj: ProfileInfoBrowserProxy) {
    instance = obj;
  }
}

let instance: ProfileInfoBrowserProxy|null = null;
