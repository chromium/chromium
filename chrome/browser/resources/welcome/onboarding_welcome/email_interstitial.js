// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview When user gets to chrome://welcome/email-interstitial, it will
 * also have a ?provider=... url param. This value is an ID that should match
 * one of the provider in the email list retrieved from the backend. If the user
 * chooses the continue button, they should be directed to the landing page of
 * that email provider.
 */
Polymer({
  is: 'email-interstitial',

  /** @private */
  welcomeBrowserProxy_: null,

  /** @override */
  ready: function() {
    this.welcomeBrowserProxy_ = welcome.WelcomeBrowserProxyImpl.getInstance();
  },

  /** @private */
  onContinueClick_: function() {
    const providerId =
        (new URL(window.location.href)).searchParams.get('provider');
    nux.NuxEmailProxyImpl.getInstance().getEmailList().then(list => {
      for (let i = 0; i < list.length; i++) {
        if (list[i].id == providerId) {
          this.welcomeBrowserProxy_.goToURL(list[i].url);
          return;
        }
      }

      // It shouldn't be possible to go to a URL with a non-existent provider
      // id.
      assertNotReached();
    });
  },

  /** @private */
  onNoThanksClick_: function() {
    this.welcomeBrowserProxy_.goToNewTabPage();
  }
});
