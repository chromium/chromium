// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UserProviderInterface} from '../personalization_app.mojom-webui';
import {PersonalizationStore} from '../personalization_store.js';

import {setDefaultUserImagesAction, setUserInfoAction} from './user_actions.js';

/**
 * @fileoverview provides functions to fetch and set user info.
 */

export async function initializeUserData(
    provider: UserProviderInterface, store: PersonalizationStore) {
  const {userInfo} = await provider.getUserInfo();
  store.dispatch(setUserInfoAction(userInfo));
}

export async function fetchDefaultUserImages(
    provider: UserProviderInterface, store: PersonalizationStore) {
  const {defaultUserImages} = await provider.getDefaultUserImages();
  store.dispatch(setDefaultUserImagesAction(defaultUserImages));
}