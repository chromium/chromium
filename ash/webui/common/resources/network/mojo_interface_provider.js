// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from '//resources/js/cr.m.js';
import {CrosNetworkConfig, CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

/** @interface */
export class MojoInterfaceProvider {
  /** @return {!CrosNetworkConfigRemote} */
  getMojoServiceRemote() {}
}

/** @implements {MojoInterfaceProvider} */
export class MojoInterfaceProviderImpl {
  constructor() {
    /** @private {?CrosNetworkConfigRemote} */
    this.remote_ = null;
  }

  /** @override */
  getMojoServiceRemote() {
    if (!this.remote_) {
      this.remote_ = CrosNetworkConfig.getRemote();
    }

    return this.remote_;
  }
}

addSingletonGetter(MojoInterfaceProviderImpl);
