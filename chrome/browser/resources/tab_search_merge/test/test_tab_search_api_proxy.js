// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabSearchApiProxy} from 'chrome://tab-search/tab_search_api_proxy.js';
import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/** @implements {TabSearchApiProxy} */
export class TestTabSearchApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getProfileTabs',
      'switchToTab',
    ]);

    /** @private {tabSearch.mojom.ProfileTabs} */
    this.profileTabs_;
  }

  /** override */
  getProfileTabs() {
    this.methodCalled('getProfileTabs');
    return Promise.resolve({profileTabs: this.profileTabs_});
  }

  /** override */
  switchToTab(tabInfo) {
    this.methodCalled('swtichToTab');
  }

  /** @param {tabSearch.mojom.ProfileTabs} profileTabs */
  setProfileTabs(profileTabs) {
    this.profileTabs_ = profileTabs;
  }
}
