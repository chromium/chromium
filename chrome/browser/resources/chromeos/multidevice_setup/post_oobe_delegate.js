// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/multidevice_setup/mojo_api.m.js';
import {MultiDeviceSetupDelegate} from 'chrome://resources/cr_components/chromeos/multidevice_setup/multidevice_setup_delegate.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
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
