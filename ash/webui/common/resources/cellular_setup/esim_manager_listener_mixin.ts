// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer mixin for observing ESimManagerObserver
 * events.
 */

import {ESimManagerObserver, ESimProfileRemote, EuiccRemote} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {dedupingMixin, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {observeESimManager} from './mojo_interface_provider.js';

type Constructor<T> = new (...args: any[]) => T;

export const ESimManagerListenerMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<ESimManagerListenerMixinInterface> => {
      // eslint-disable-next-line @typescript-eslint/naming-convention
      class ESimManagerListenerMixin extends superClass implements
          ESimManagerListenerMixinInterface {
        private observer_: ESimManagerObserver|null = null;

        override connectedCallback() {
          super.connectedCallback();

          observeESimManager(this);
        }

        // ESimManagerObserver methods. Override these in the implementation.
        onAvailableEuiccListChanged(): void {}
        onProfileListChanged(): void {}
        onEuiccChanged(): void {}
        onProfileChanged(): void {}
      }

      return ESimManagerListenerMixin;
    });

// eslint-disable-next-line @typescript-eslint/naming-convention
export interface ESimManagerListenerMixinInterface {
  onAvailableEuiccListChanged(): void;
  onProfileListChanged(euicc: EuiccRemote): void;
  onEuiccChanged(euicc: EuiccRemote): void;
  onProfileChanged(profile: ESimProfileRemote): void;
}
