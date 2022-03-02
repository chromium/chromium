
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-visibility-page' component is part of the Nearby
 * Share onboarding flow. It allows users to setup their visibility preference
 * while enabling the feature for the first time.
 *
 * It is embedded in chrome://os-settings, chrome://settings and as a standalone
 * dialog via chrome://nearby.
 */
Polymer({
  is: 'nearby-visibility-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?nearby_share.NearbySettings} */
    settings: {
      type: Object,
      notify: true,
    },

    /** @private */
    isVisibilitySelected_: {
      type: Boolean,
      notify: true,
    },
  },

  listeners: {
    'next': 'onNext_',
    'manage-contacts': 'onManageContacts_',
    'close': 'onClose_',
    'view-enter-start': 'onVisibilityViewEnterStart_',
  },

  /**
   * Determines if the feature flag for One-page onboarding workflow is enabled.
   * @return {boolean} whether the one-page onboarding is enabled
   * @private
   */
  isOnePageOnboardingEnabled_() {
    return loadTimeData.getBoolean('isOnePageOnboardingEnabled');
  },

  /** @private */
  onNext_() {
    const contactVisibility = /** @type {NearbyContactVisibilityElement} */
        (this.$.contactVisibility);
    contactVisibility.saveVisibilityAndAllowedContacts();
    this.set('settings.isOnboardingComplete', true);
    this.set('settings.enabled', true);
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingCompleteMetrics(
          NearbyShareOnboardingFinalState.VISIBILITY_PAGE,
          contactVisibility.getSelectedVisibility());
    } else {
      processOnboardingCompleteMetrics();
    }

    this.fire('onboarding-complete');
  },

  /** @private */
  onClose_() {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingCancelledMetrics(
          NearbyShareOnboardingFinalState.VISIBILITY_PAGE);
    } else {
      processOnboardingCancelledMetrics(
          NearbyShareOnboardingFinalState.VISIBILITY_PAGE);
    }
    this.fire('onboarding-cancelled');
  },

  /** @private */
  onVisibilityViewEnterStart_() {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingVisibilityPageShownMetrics();
    }
  },

  /** @private */
  onManageContacts_() {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingManageContactsMetrics();
    }
    window.open(this.i18n('nearbyShareManageContactsUrl'), '_blank');
  },

});
