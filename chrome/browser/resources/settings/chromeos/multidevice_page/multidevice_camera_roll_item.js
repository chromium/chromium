// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-camera-roll-item' encapsulates
 * special logic for the PhoneHub Camera Roll item used in the multidevice
 * subpage.
 *
 * Camera Roll depends on file access permissions on the connected Android
 * device.
 *
 * If permission is granted the multidevice feature item is used in the standard
 * way, otherwise the feature-controller and feature-summary slots are
 * overridden with a disabled toggle and the Camera Roll localized string
 * component contains a learn more link.
 */
Polymer({
  is: 'settings-multidevice-camera-roll-item',

  behaviors: [
    MultiDeviceFeatureBehavior,
  ],

  /**
   * @return {boolean}
   * @private
   */
  isCameraRollFilePermissionGranted_() {
    return this.pageContentData.isCameraRollFilePermissionGranted;
  },

  /**
   * @return {string}
   * @private
   */
  getFeatureDisabledSummary_() {
    return this.i18nAdvanced(
        'multidevicePhoneHubCameraRollDisabledItemSummary');
  },
});
