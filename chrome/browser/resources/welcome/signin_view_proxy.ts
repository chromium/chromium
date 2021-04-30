// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const NUX_SIGNIN_VIEW_INTERACTION_METRIC_NAME =
    'FirstRun.NewUserExperience.SignInInterstitialInteraction';

enum NuxSignInInterstitialInteractions {
  PageShown = 0,
  NavigatedAway,
  Skip,
  SignIn,
  NavigatedAwayThroughBrowserHistory,
}

const NUX_SIGNIN_VIEW_INTERACTIONS_COUNT =
    Object.keys(NuxSignInInterstitialInteractions).length;

export interface SigninViewProxy {
  recordPageShown(): void;
  recordNavigatedAway(): void;
  recordNavigatedAwayThroughBrowserHistory(): void;
  recordSkip(): void;
  recordSignIn(): void;
}

export class SigninViewProxyImpl implements SigninViewProxy {
  recordPageShown() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.PageShown);
  }

  recordNavigatedAway() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.NavigatedAway);
  }

  recordNavigatedAwayThroughBrowserHistory() {
    this.recordInteraction_(
        NuxSignInInterstitialInteractions.NavigatedAwayThroughBrowserHistory);
  }

  recordSkip() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.Skip);
  }

  recordSignIn() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.SignIn);
  }

  private recordInteraction_(interaction: number): void {
    chrome.metricsPrivate.recordEnumerationValue(
        NUX_SIGNIN_VIEW_INTERACTION_METRIC_NAME, interaction,
        NUX_SIGNIN_VIEW_INTERACTIONS_COUNT);
  }

  static getInstance(): SigninViewProxy {
    return instance || (instance = new SigninViewProxyImpl());
  }

  static setInstance(obj: SigninViewProxy) {
    instance = obj;
  }
}

let instance: SigninViewProxy|null = null;
