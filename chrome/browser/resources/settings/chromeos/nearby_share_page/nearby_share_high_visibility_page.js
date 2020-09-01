// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-high-visibility-page' component is opened when the
 * user is broadcast in high-visibility mode. The user may cancel to stop high
 * visibility mode at any time.
 */
Polymer({
  is: 'nearby-share-high-visibility-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {string}
     */
    deviceName: {
      notify: true,
      type: String,
      value: 'DEVICE_NAME_NOT_SET',
    }
  },

  /**
   * @return {string} localized string
   * @private
   */
  getSubTitle_() {
    return this.i18n('nearbyShareHighVisibilitySubTitle', this.deviceName);
  },
});
