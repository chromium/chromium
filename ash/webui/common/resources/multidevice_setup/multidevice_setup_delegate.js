// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface which provides the ability to set the host device and perform
 * related logic.
 * @interface
 */
export class MultiDeviceSetupDelegate {
  /** @return {boolean} */
  isPasswordRequiredToSetHost() {}

  /**
   * @param {string} hostInstanceIdOrLegacyDeviceId The ID of the host to set.
   * @param {string=} opt_authToken An auth token to authenticate the request;
   *     only necessary if isPasswordRequiredToSetHost() returns true.
   * @return {!Promise<{success: boolean}>}
   * TODO(crbug.com/40105247): When v1 DeviceSync is turned off, only
   * use Instance ID since all devices are guaranteed to have one.
   */
  setHostDevice(hostInstanceIdOrLegacyDeviceId, opt_authToken) {}

  /** @return {boolean} */
  shouldExitSetupFlowAfterSettingHost() {}

  /** @return {string} */
  getStartSetupCancelButtonTextId() {}
}
