// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {BigBuffer, BigBufferSharedMemoryRegion} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

import {UserProviderInterface} from '../../personalization_app.mojom-webui.js';
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

export function saveCameraImage(
    pngBinary: Uint8Array, userProvider: UserProviderInterface) {
  const numBytes = pngBinary.byteLength;

  const {handle, result: createSharedBufferResult} =
      Mojo.createSharedBuffer(numBytes);
  assert(
      createSharedBufferResult === Mojo.RESULT_OK,
      'Could not create shared buffer');

  const {buffer, result: mapBufferResult} = handle.mapBuffer(0, numBytes);
  assert(mapBufferResult === Mojo.RESULT_OK, 'Could not map shared buffer');

  const uint8View = new Uint8Array(buffer);
  uint8View.set(pngBinary);

  const sharedMemory:
      BigBufferSharedMemoryRegion = {bufferHandle: handle, size: numBytes};
  // Cast to any first because types inferred from generated closure compiler
  // annotations are incorrect.
  const bigBuffer: BigBuffer = {sharedMemory} as any;

  userProvider.selectCameraImage(bigBuffer);
}
