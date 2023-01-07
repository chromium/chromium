// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const NUX_SIGNIN_VIEW_INTERACTION_METRIC_NAME =
    'FirstRun.NewUserExperience.SignInInterstitialInteraction';

enum NuxSignInInterstitialInteractions {
  PAGE_SHOWN = 0,
  NAVIGATED_AWAY,
  SKIP,
  SIGN_IN,
  NAVIGATED_AWAY_THROUGH_BROWSER_HISTORY,
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
    this.recordInteraction_(NuxSignInInterstitialInteractions.PAGE_SHOWN);
  }

  recordNavigatedAway() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.NAVIGATED_AWAY);
  }

  recordNavigatedAwayThroughBrowserHistory() {
    this.recordInteraction_(NuxSignInInterstitialInteractions
                                .NAVIGATED_AWAY_THROUGH_BROWSER_HISTORY);
  }

  recordSkip() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.SKIP);
  }

  recordSignIn() {
    this.recordInteraction_(NuxSignInInterstitialInteractions.SIGN_IN);
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
