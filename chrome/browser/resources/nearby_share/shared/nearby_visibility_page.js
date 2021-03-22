
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
    'close': 'onClose_'
  },

  /** @private */
  onNext_() {
    const contactVisibility = /** @type {NearbyContactVisibilityElement} */
        (this.$.contactVisibility);
    contactVisibility.saveVisibilityAndAllowedContacts();
    this.set('settings.enabled', true);
    processOnboardingCompleteMetrics();
    this.fire('onboarding-complete');
  },

  /** @private */
  onClose_() {
    processOnboardingCancelledMetrics(
        NearbyShareOnboardingFinalState.VISIBILITY_PAGE);
    this.fire('onboarding-cancelled');
  },

  /** @private */
  onManageContacts_() {
    window.open(this.i18n('nearbyShareManageContactsUrl'), '_blank');
  },

});
