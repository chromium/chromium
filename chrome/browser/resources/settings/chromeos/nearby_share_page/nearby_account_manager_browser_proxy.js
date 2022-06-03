// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

// TODO(crbug.com/1123712): Add a message handler for this class instead of
// implicitly relying on the People Page.
cr.define('nearby_share', function() {
  /**
   * Information for an account managed by Chrome OS AccountManager.
   * @typedef {{
   *   id: string,
   *   accountType: number,
   *   isDeviceAccount: boolean,
   *   isSignedIn: boolean,
   *   unmigrated: boolean,
   *   fullName: string,
   *   email: string,
   *   pic: string,
   *   organization: (string|undefined),
   * }}
   */
  /* #export */ let Account;

  /** @interface */
  /* #export */ class NearbyAccountManagerBrowserProxy {
    /**
     * Returns a Promise for the list of GAIA accounts held in AccountManager.
     * @return {!Promise<!Array<nearby_share.Account>>}
     */
    getAccounts() {}
  }

  /**
   * @implements {nearby_share.NearbyAccountManagerBrowserProxy}
   */
  /* #export */ class NearbyAccountManagerBrowserProxyImpl {
    /** @override */
    getAccounts() {
      return cr.sendWithPromise('getAccounts');
    }
  }

  cr.addSingletonGetter(NearbyAccountManagerBrowserProxyImpl);

  // #cr_define_end
  return {
    Account,
    NearbyAccountManagerBrowserProxy,
    NearbyAccountManagerBrowserProxyImpl,
  };
});
