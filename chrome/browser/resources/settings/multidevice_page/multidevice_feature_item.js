// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item for an individual multidevice feature. These features appear in the
 * multidevice subpage to allow the user to individually toggle them as long as
 * the phone is enabled as a multidevice host. The feature items contain basic
 * information relevant to the individual feature, such as a route to the
 * feature's autonomous page if there is one.
 */
cr.exportPath('settings');

Polymer({
  is: 'settings-multidevice-feature-item',

  behaviors: [MultiDeviceFeatureBehavior],

  properties: {
    /** @type {!settings.MultiDeviceFeature} */
    feature: Number,

    /**
     * If it is truthy, the item should be actionable and clicking on it should
     * navigate to the provided route. Otherwise, the item is simply not
     * actionable.
     * @type {settings.Route|undefined}
     */
    subpageRoute: Object,

    /**
     * URLSearchParams for subpage route. No param is provided if it is
     * undefined.
     * @type {URLSearchParams|undefined}
     */
    subpageRouteUrlSearchParams: Object,
  },

  /**
   * @return {boolean}
   * @private
   */
  hasSubpageClickHandler_: function() {
    return !!this.subpageRoute && this.isFeatureAllowedByPolicy(this.feature);
  },

  /** @private */
  handleItemClick_: function(event) {
    if (!this.hasSubpageClickHandler_())
      return;

    // We do not navigate away if the click was on a link.
    if (event.path[0].tagName === 'A') {
      event.stopPropagation();
      return;
    }

    settings.navigateTo(this.subpageRoute, this.subpageRouteUrlSearchParams);
  },
});
