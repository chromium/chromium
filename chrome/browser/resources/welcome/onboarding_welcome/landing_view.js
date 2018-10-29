// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'landing-view',

  /** @private */
  onExistingUserClick_: function() {
    welcome.WelcomeBrowserProxyImpl.getInstance().handleActivateSignIn(
        'chrome://welcome/returning-user');
  },

  /** @private */
  onNewUserClick_: function() {
    welcome.navigateTo(welcome.Routes.NEW_USER, 1);
  }
});
