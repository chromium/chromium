// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Element containing cellular network banner. This UI is used during inhibit
 * (busy) state, some cellular operations are not allowed in this state, it
 * informs the user of the current inhibit state and how long it will
 * take to finish.
 */
Polymer({
  is: 'cellular-banner',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * @type {!OncMojo.DeviceStateProperties}
     */
    deviceState: Object,
  },

  /**
   * @return {string} banner message.
   * @private
   */
  getBannerMessage_() {
    if (!this.deviceState) {
      return '';
    }
    // If current state is unInhibited cellular banner should not be attached to
    // UI. Parent UI element should remove it.
    const mojom = chromeos.networkConfig.mojom.InhibitReason;
    const inhibitReason = this.deviceState.inhibitReason;

    switch (inhibitReason) {
      case mojom.kInstallingProfile:
        return this.i18n('cellularNetworkInstallingProfile');
      case mojom.kRenamingProfile:
        return this.i18n('cellularNetworkRenamingProfile');
      case mojom.kRemovingProfile:
        return this.i18n('cellularNetworkRemovingProfile');
      case mojom.kConnectingToProfile:
        return this.i18n('cellularNetworkConnectingToProfile');
      case mojom.kRefreshingProfileList:
        return this.i18n('cellularNetworRefreshingProfileListProfile');
    }

    assertNotReached();
    return '';
  }
});
