// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'signin-view',

  behaviors: [welcome.NavigationBehavior],

  // TODO(scottchen): read this from a nux onboarding feature param via
  //     loadTimeData.
  shouldShowEmailInterstitial_: true,

  /**
   * When the user clicks sign-in, check whether or not they previously
   * selected an email provider they prefer to use. If so, direct them back to
   * the email-interstitial page, otherwise let it direct to NTP.
   * @private
   */
  onSignInClick_: function() {
    let redirectUrl = null;

    const savedProvider =
        nux.NuxEmailProxyImpl.getInstance().getSavedProvider();
    if (savedProvider != undefined && this.shouldShowEmailInterstitial_) {
      redirectUrl =
          `chrome://welcome/email-interstitial?provider=${savedProvider}`;
    }

    welcome.WelcomeBrowserProxyImpl.getInstance().handleActivateSignIn(
        redirectUrl);
  },

  /** @private */
  onNoThanksClick_: function() {
    welcome.navigateToNextStep();
  }
});
