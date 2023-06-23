// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin to be used by Polymer elements that want to listen for
 * Passpoint subscription events.
 */

import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {PasspointEventsListenerReceiver, PasspointSubscription} from 'chrome://resources/ash/common/connectivity/passpoint.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

type Constructor<T> = new (...args: any[]) => T;

export interface PasspointListenerMixinInterface {
  isPasspointSettingsEnabled: boolean;
  onPasspointSubscriptionAdded(subscription: PasspointSubscription): void;
  onPasspointSubscriptionRemoved(subscription: PasspointSubscription): void;
}

export const PasspointListenerMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PasspointListenerMixinInterface> => {
      class PasspointListenerMixin extends superClass implements
          PasspointListenerMixinInterface {
        static get properties() {
          return {
            isPasspointSettingsEnabled: {
              type: Boolean,
              readOnly: true,
              value() {
                return loadTimeData.valueExists('isPasspointSettingsEnabled') &&
                    loadTimeData.getBoolean('isPasspointSettingsEnabled');
              },
            },
          };
        }

        isPasspointSettingsEnabled: boolean;
        private listener_: PasspointEventsListenerReceiver|null;

        constructor(...args: any[]) {
          super(...args);

          this.listener_ = null;
        }

        override connectedCallback() {
          super.connectedCallback();

          if (this.isPasspointSettingsEnabled) {
            this.listener_ = new PasspointEventsListenerReceiver(this);
            MojoConnectivityProvider.getInstance()
                .getPasspointService()
                .registerPasspointListener(
                    this.listener_.$.bindNewPipeAndPassRemote());
          }
        }

        override disconnectedCallback() {
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
