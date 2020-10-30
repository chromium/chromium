// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1125474): Rename file to guest_tab.js once all audit is done
// and all instances of non-ephemeral Guest profiles are deprecated.

// clang-format off
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
class GuestSignIn {
  /**
   * Handles sign in and sign out based on user's sign in status.
   */
  onChangeSignInStatusClicked() {}
}

/** @implements {GuestSignIn} */
class GuestSignInImpl {
  onChangeSignInStatusClicked() {
    chrome.send('onChangeSignInStatusClicked');
  }
}

addSingletonGetter(GuestSignInImpl);

window.addEventListener('load', function() {
  $('change-sign-in-status').onclick = () =>
      GuestSignInImpl.getInstance().onChangeSignInStatusClicked();
});
