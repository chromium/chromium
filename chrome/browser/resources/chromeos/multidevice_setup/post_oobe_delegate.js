// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/multidevice_setup/mojo_api.js';
import {MultiDeviceSetupDelegate} from 'chrome://resources/ash/common/multidevice_setup/multidevice_setup_delegate.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @implements {MultiDeviceSetupDelegate} */
export class PostOobeDelegate {
  /** @override */
  isPasswordRequiredToSetHost() {
    return true;
  }

  /** @override */
  setHostDevice(hostInstanceIdOrLegacyDeviceId, opt_authToken) {
    // An authentication token is required to set the host device post-OOBE.
    assert(!!opt_authToken);

    // Note: A cast is needed here because currently all Mojo functions which
    // return a promise are typed only as {Promise}. The setHostDevice()
    // function always returns a {!Promise<{success: boolean}>} (see
    // multidevice_setup.mojom).
    return /** @type {!Promise<{success: boolean}>} */ (
        MojoInterfaceProviderImpl.getInstance()
            .getMojoServiceRemote()
            .setHostDevice(hostInstanceIdOrLegacyDeviceId, opt_authToken));
  }

  /** @override */
  shouldExitSetupFlowAfterSettingHost() {
    return false;
  }

  /** @override */
  getStartSetupCancelButtonTextId() {
    return 'cancel';
  }
}
