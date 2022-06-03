// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const NUX_LANDING_PAGE_INTERACTION_METRIC_NAME =
    'FirstRun.NewUserExperience.LandingPageInteraction';

enum NuxLandingPageInteractions {
  PageShown = 0,
  NavigatedAway,
  NewUser,
  ExistingUser,
}

const NUX_LANDING_PAGE_INTERACTIONS_COUNT =
    Object.keys(NuxLandingPageInteractions).length;

export interface LandingViewProxy {
  recordPageShown(): void;
  recordNavigatedAway(): void;
  recordNewUser(): void;
  recordExistingUser(): void;
}

export class LandingViewProxyImpl implements LandingViewProxy {
  recordPageShown() {
    this.recordInteraction_(NuxLandingPageInteractions.PageShown);
  }

  recordNavigatedAway() {
    this.recordInteraction_(NuxLandingPageInteractions.NavigatedAway);
  }

  recordNewUser() {
    this.recordInteraction_(NuxLandingPageInteractions.NewUser);
  }

  recordExistingUser() {
    this.recordInteraction_(NuxLandingPageInteractions.ExistingUser);
  }

  private recordInteraction_(interaction: number) {
    chrome.metricsPrivate.recordEnumerationValue(
        NUX_LANDING_PAGE_INTERACTION_METRIC_NAME, interaction,
        NUX_LANDING_PAGE_INTERACTIONS_COUNT);
  }

  static getInstance(): LandingViewProxy {
    return instance || (instance = new LandingViewProxyImpl());
  }

  static setInstance(obj: LandingViewProxy) {
    instance = obj;
  }
}

let instance: LandingViewProxy|null = null;
