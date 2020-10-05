// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-onboarding-page' component handles the Nearby Share
 * onboarding flow. It is embedded in chrome://os-settings, chrome://settings
 * and as a standalone dialog via chrome://nearby.
 */
Polymer({
  is: 'nearby-onboarding-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?nearby_share.NearbySettings} */
    settings: {
      type: Object,
    }
  },

  listeners: {
    'next': 'onNext_',
  },

  /**
   * @private
   */
  onNext_() {
    this.set('settings.deviceName', this.$.deviceName.value);
    this.fire('change-page', {page: 'visibility'});
  },
});
