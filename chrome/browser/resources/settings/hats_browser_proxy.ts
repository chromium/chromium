// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles Happiness Tracking Surveys for the settings pages. */

/**
 * All Trust & Safety based interactions which may result in a HaTS survey.
 *
 * Must be kept in sync with the enum of the same name in hats_handler.h.
 */
export enum TrustSafetyInteraction {
  RAN_SAFETY_CHECK = 0,
  USED_PRIVACY_CARD = 1,
  OPENED_PRIVACY_SANDBOX = 2,
  OPENED_PASSWORD_MANAGER = 3,
  COMPLETED_PRIVACY_GUIDE = 4,
  RAN_PASSWORD_CHECK = 5,
  OPENED_AD_PRIVACY = 6,
  OPENED_TOPICS_SUBPAGE = 7,
  OPENED_FLEDGE_SUBPAGE = 8,
  OPENED_AD_MEASUREMENT_SUBPAGE = 9,
}

export interface HatsBrowserProxy {
  /**
   * Inform HaTS that the user performed a Trust & Safety interaction.
   * @param interaction The type of interaction performed by the user.
   */
  trustSafetyInteractionOccurred(interaction: TrustSafetyInteraction): void;
}

export class HatsBrowserProxyImpl implements HatsBrowserProxy {
  trustSafetyInteractionOccurred(interaction: TrustSafetyInteraction) {
    chrome.send('trustSafetyInteractionOccurred', [interaction]);
  }

  static getInstance(): HatsBrowserProxy {
    return instance || (instance = new HatsBrowserProxyImpl());
  }

  static setInstance(obj: HatsBrowserProxy) {
    instance = obj;
  }
}

let instance: HatsBrowserProxy|null = null;
