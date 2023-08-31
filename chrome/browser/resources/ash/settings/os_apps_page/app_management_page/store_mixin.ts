// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines StoreClient, a Polymer mixin to tie a front-end
 * element to back-end data from the store.
 */

import {makeStoreClientMixin, StoreClientInterface} from 'chrome://resources/cr_elements/store_client/store_client.js';

import {AppManagementActions} from './actions.js';
import {AppManagementPageState, AppManagementStore} from './store.js';

export interface AppManagementStoreMixinInterface extends
    StoreClientInterface<AppManagementPageState, AppManagementActions> {}

export const AppManagementStoreMixin =
    makeStoreClientMixin(AppManagementStore.getInstance);
