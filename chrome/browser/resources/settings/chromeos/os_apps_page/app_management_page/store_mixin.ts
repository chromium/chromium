// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines StoreClient, a Polymer mixin to tie a front-end
 * element to back-end data from the store.
 */

import {StoreClient, StoreClientInterface} from 'chrome://resources/ash/common/store/store_client.js';
import {dedupingMixin, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementPageState, AppManagementStore} from './store.js';

type Constructor<T> = new (...args: any[]) => T;

type ValueGetterFn<T = any> = (state: T) => any;
type WatchFn = (localProperty: string, valueGetter: ValueGetterFn) => void;

export interface AppManagementStoreMixinInterface extends
    StoreClientInterface<AppManagementPageState> {
  watch: WatchFn;
  getState(): AppManagementPageState;
  getStore(): AppManagementStore;
}

interface StoreClientPrivateInterface {
  watch_: WatchFn;
}

export const AppManagementStoreMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<AppManagementStoreMixinInterface> => {
      const superclassBase = mixinBehaviors([StoreClient], superClass) as T &
          Constructor<StoreClientInterface<AppManagementPageState>&
                      StoreClientPrivateInterface>;

      class AppManagementStoreClientInternal extends superclassBase implements
          AppManagementStoreMixinInterface {
        override watch(
            localProperty: string,
            valueGetter: ValueGetterFn<AppManagementPageState>) {
          this.watch_(localProperty, valueGetter);
        }

        override getState(): AppManagementPageState {
          return this.getStore().data;
        }

        override getStore(): AppManagementStore {
          return AppManagementStore.getInstance();
        }
      }

      return AppManagementStoreClientInternal;
    });
