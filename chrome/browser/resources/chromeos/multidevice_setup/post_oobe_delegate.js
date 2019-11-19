// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup', function() {
  /** @implements {multidevice_setup.MultiDeviceSetupDelegate} */
  class PostOobeDelegate {
    /** @override */
    isPasswordRequiredToSetHost() {
      return true;
    }

    /** @override */
    setHostDevice(hostDeviceId, opt_authToken) {
      // An authentication token is required to set the host device post-OOBE.
      assert(!!opt_authToken);

      // Note: A cast is needed here because currently all Mojo functions which
      // return a promise are typed only as {Promise}. The setHostDevice()
      // function always returns a {!Promise<{success: boolean}>} (see
      // multidevice_setup.mojom).
      return /** @type {!Promise<{success: boolean}>} */ (
          multidevice_setup.MojoInterfaceProviderImpl.getInstance()
              .getMojoServiceRemote()
              .setHostDevice(hostDeviceId, opt_authToken));
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

  return {
    PostOobeDelegate: PostOobeDelegate,
  };
});
