// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  /** @return {!MojoInterfaceProvider} */
  static getInstance() {
    return instance || (instance = new MojoInterfaceProviderImpl());
  }

  /** @param {!MojoInterfaceProvider} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?MojoInterfaceProvider} */
let instance = null;
