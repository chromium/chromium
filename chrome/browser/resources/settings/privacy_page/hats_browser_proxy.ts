// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles Happiness Tracking Surveys for the settings pages. */

import type {SafeBrowsingSetting} from './safe_browsing_types.js';

/**
 * All Trust & Safety based interactions which may result in a HaTS survey.
 *
 * Must be kept in sync with the enum of the same name in hats_handler.h.
 */
export enum TrustSafetyInteraction {
  RAN_SAFETY_CHECK = 0,
  USED_PRIVACY_CARD = 1,
  // OPENED_PRIVACY_SANDBOX = 2, // DEPRECATED
  OPENED_PASSWORD_MANAGER = 3,
  COMPLETED_PRIVACY_GUIDE = 4,
  RAN_PASSWORD_CHECK = 5,
  // OPENED_AD_PRIVACY = 6, // DEPRECATED
  // OPENED_TOPICS_SUBPAGE = 7, // DEPRECATED
  // OPENED_FLEDGE_SUBPAGE = 8, // DEPRECATED
  // OPENED_AD_MEASUREMENT_SUBPAGE = 9, // DEPRECATED
  // OPENED_GET_MOST_CHROME = 10, // DEPRECATED
}

/**
 * Enumeration of interactions with the security settings v2 page. Must be kept
 * in sync with the enum of the same name located in:
 * chrome/browser/ui/webui/settings/hats_handler.h
 */
export enum SecurityPageV2Interaction {
  STANDARD_BUNDLE_RADIO_BUTTON_CLICK = 0,
  ENHANCED_BUNDLE_RADIO_BUTTON_CLICK = 1,
  SAFE_BROWSING_ROW_EXPANDED = 2,
  STANDARD_SAFE_BROWSING_RADIO_BUTTON_CLICK = 3,
  ENHANCED_SAFE_BROWSING_RADIO_BUTTON_CLICK = 4,
}

/** Enumeration of all security settings bundle modes.*/
// LINT.IfChange(SecuritySettingsBundleSetting)
export enum SecuritySettingsBundleSetting {
  STANDARD = 0,
  ENHANCED = 1,
}
// LINT.ThenChange(/components/safe_browsing/core/common/safe_browsing_prefs.h:SecuritySettingsBundleSetting)

/**
 * All interactions from the security settings page which may result in a HaTS
 * survey.
 *
 * Must be kept in sync with the enum of the same name in hats_handler.h.
 */
export interface HatsBrowserProxy {
  /**
   * Inform HaTS that the user performed a Trust & Safety interaction.
   * @param interaction The type of interaction performed by the user.
   */
  trustSafetyInteractionOccurred(interaction: TrustSafetyInteraction): void;

  /**
   * Inform HaTS that the user visited the security page.
   * @param securityPageInteractions The interactions performed on the security
   *     page.
   * @param safeBrowsingSetting The safe browsing settings the user had
   *      when they opened the security page.
   * @param totalTimeOnPage The amount of time the user spent on the security
   *     page.
   * @param securitySettingsBundleSetting The security settings bundle the user
   *     had when they opened the security page.
   */
  securityPageHatsRequest(
      securityPageInteractions: SecurityPageV2Interaction[],
      safeBrowsingSetting: SafeBrowsingSetting, totalTimeOnPage: number,
      securitySettingsBundleSetting: SecuritySettingsBundleSetting): void;

  /**
   * Returns the current date value.
   */
  now(): number;
}

export class HatsBrowserProxyImpl implements HatsBrowserProxy {
  trustSafetyInteractionOccurred(interaction: TrustSafetyInteraction) {
    chrome.send('trustSafetyInteractionOccurred', [interaction]);
  }

  securityPageHatsRequest(
      securityPageInteractions: SecurityPageV2Interaction[],
      safeBrowsingSetting: SafeBrowsingSetting, totalTimeOnPage: number,
      securitySettingsBundleSetting: SecuritySettingsBundleSetting) {
    chrome.send('securityPageHatsRequest', [
      securityPageInteractions,
      safeBrowsingSetting,
      totalTimeOnPage,
      securitySettingsBundleSetting,
    ]);
  }

  now() {
    return window.performance.now();
  }

  static getInstance(): HatsBrowserProxy {
    return instance || (instance = new HatsBrowserProxyImpl());
  }

  static setInstance(obj: HatsBrowserProxy) {
    instance = obj;
  }
}

let instance: HatsBrowserProxy|null = null;
