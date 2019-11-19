// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

const NUX_SIGNIN_VIEW_INTERACTION_METRIC_NAME =
    'FirstRun.NewUserExperience.SignInInterstitialInteraction';

/**
 * NuxSignInInterstitialInteractions enum.
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
const NuxSignInInterstitialInteractions = {
  PageShown: 0,
  NavigatedAway: 1,
  Skip: 2,
  SignIn: 3,
  NavigatedAwayThroughBrowserHistory: 4,
};

const NUX_SIGNIN_VIEW_INTERACTIONS_COUNT =
    Object.keys(NuxSignInInterstitialInteractions).length;

/** @interface */
export class SigninViewProxy {
  recordPageShown() {}
  recordNavigatedAway() {}
  recordNavigatedAwayThroughBrowserHistory() {}
  recordSkip() {}
  recordSignIn() {}
}

/** @implements {SigninViewProxy} */
export class SigninViewProxyImpl {
  /** @override */
  recordPageShown() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.PageShown);
  }

  /** @override */
  recordNavigatedAway() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.NavigatedAway);
  }

  /** @override */
  recordNavigatedAwayThroughBrowserHistory() {
    this.recordInteraction_(
        NuxSignInInterstitialInteractions.NavigatedAwayThroughBrowserHistory);
  }

  /** @override */
  recordSkip() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.Skip);
  }

  /** @override */
  recordSignIn() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.SignIn);
  }

  /**
   * @param {number} interaction
   * @private
   */
  recordInteraction_(interaction) {
    chrome.metricsPrivate.recordEnumerationValue(
        NUX_SIGNIN_VIEW_INTERACTION_METRIC_NAME, interaction,
        NUX_SIGNIN_VIEW_INTERACTIONS_COUNT);
  }
}

addSingletonGetter(SigninViewProxyImpl);
