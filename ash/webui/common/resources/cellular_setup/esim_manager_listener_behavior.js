// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for observing ESimManagerObserver
 * events.
 */

import {ESimManagerObserver, ESimProfileRemote, EuiccRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {observeESimManager} from './mojo_interface_provider.js';

/** @polymerBehavior */
export const ESimManagerListenerBehavior = {
  /** @private {?ESimManagerObserver} */
  observer_: null,

  /** @override */
  attached() {
    observeESimManager(this);
  },

  // ESimManagerObserver methods. Override these in the implementation.

  onAvailableEuiccListChanged() {},

  /**
   * @param {!EuiccRemote} euicc
   */
  onProfileListChanged(euicc) {},

  /**
   * @param {!EuiccRemote} euicc
   */
  onEuiccChanged(euicc) {},

  /**
   * @param {!ESimProfileRemote} profile
   */
  onProfileChanged(profile) {},
};

/** @interface */
export class ESimManagerListenerBehaviorInterface {
  onAvailableEuiccListChanged() {}

  /**
   * @param {!EuiccRemote} euicc
   */
  onProfileListChanged(euicc) {}

  /**
   * @param {!EuiccRemote} euicc
   */
  onEuiccChanged(euicc) {}

  /**
   * @param {!ESimProfileRemote} profile
   */
  onProfileChanged(profile) {}
}
