
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
    }
  },

  onNextTap_() {
    this.set('settings.enabled', true);
    this.fire('change-page', {page: 'discovery'});
  },

  onCloseTap_() {
    this.fire('close');
  },

  onManageContactsTap_() {
    // TODO(vecore): this is not a final link
    window.open('https://contacts.google.com');
  },

});
