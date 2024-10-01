// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin to be used by Polymer elements that want to listen for
 * Passpoint subscription events.
 */

import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {PasspointEventsListenerReceiver, PasspointSubscription} from 'chrome://resources/ash/common/connectivity/passpoint.mojom-webui.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export interface PasspointListenerMixinInterface {
  onPasspointSubscriptionAdded(subscription: PasspointSubscription): void;
  onPasspointSubscriptionRemoved(subscription: PasspointSubscription): void;
}

export const PasspointListenerMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PasspointListenerMixinInterface> => {
      class PasspointListenerMixin extends superClass implements
          PasspointListenerMixinInterface {
        private listener_: PasspointEventsListenerReceiver|null;

        constructor(...args: any[]) {
          super(...args);

          this.listener_ = null;
        }

        override connectedCallback(): void {
          super.connectedCallback();

          this.listener_ = new PasspointEventsListenerReceiver(this);
          MojoConnectivityProvider.getInstance()
              .getPasspointService()
              .registerPasspointListener(
                  this.listener_.$.bindNewPipeAndPassRemote());
        }

        override disconnectedCallback(): void {
          super.disconnectedCallback();

          if (this.listener_) {
            this.listener_.$.close();
          }
        }

        onPasspointSubscriptionAdded(_: PasspointSubscription): void {}
        onPasspointSubscriptionRemoved(_: PasspointSubscription): void {}
      }
      return PasspointListenerMixin;
    });
