// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles Happiness Tracking Surveys for the settings pages. */

// clang-format on
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format off

/**
 * All Trust & Safety based interactions which may result in a HaTS survey.
 *
 * Must be kept in sync with the enum of the same name in hats_handler.h.
 * @enum {number}
 */
export const TrustSafetyInteraction = {
  RAN_SAFETY_CHECK: 0,
  USED_PRIVACY_CARD: 1,
  OPENED_PRIVACY_SANDBOX: 2,
  OPENED_PASSWORD_MANAGER: 3,
};

/** @interface */
export class HatsBrowserProxy {
  /**
   * Inform HaTS that the user performed a Trust & Safety interaction.
   * @param {TrustSafetyInteraction} interaction The type of interaction
   *    performed by the user.
   */
  trustSafetyInteractionOccurred(interaction) {}
}

/** @implements {HatsBrowserProxy} */
export class HatsBrowserProxyImpl {
  /** @override*/
  trustSafetyInteractionOccurred(interaction) {
    chrome.send('trustSafetyInteractionOccurred', [interaction]);
  }
}

addSingletonGetter(HatsBrowserProxyImpl);
