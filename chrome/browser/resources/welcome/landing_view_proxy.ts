// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const NUX_LANDING_PAGE_INTERACTION_METRIC_NAME =
    'FirstRun.NewUserExperience.LandingPageInteraction';

enum NuxLandingPageInteractions {
  PAGE_SHOWN = 0,
  NAVIGATED_AWAY,
  NEW_USER,
  EXISTING_USER,
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
    this.recordInteraction_(NuxLandingPageInteractions.PAGE_SHOWN);
  }

  recordNavigatedAway() {
    this.recordInteraction_(NuxLandingPageInteractions.NAVIGATED_AWAY);
  }

  recordNewUser() {
    this.recordInteraction_(NuxLandingPageInteractions.NEW_USER);
  }

  recordExistingUser() {
    this.recordInteraction_(NuxLandingPageInteractions.EXISTING_USER);
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
