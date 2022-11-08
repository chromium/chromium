// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';
import {MultiDeviceSetup, MultiDeviceSetupRemote} from 'chrome://resources/mojo/chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-webui.js';

/** @interface */
export class MojoInterfaceProvider {
  /**
   * @return {!MultiDeviceSetupRemote}
   */
  getMojoServiceRemote() {}
}

/** @implements {MojoInterfaceProvider} */
export class MojoInterfaceProviderImpl {
  constructor() {
    /** @private {?MultiDeviceSetupRemote} */
    this.remote_ = null;
  }

  /** @override */
  getMojoServiceRemote() {
    if (!this.remote_) {
      this.remote_ = MultiDeviceSetup.getRemote();
    }

    return this.remote_;
  }
}

addSingletonGetter(MojoInterfaceProviderImpl);
