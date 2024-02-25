// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {UserImage} from '../../personalization_app.mojom-webui.js';
import {PersonalizationState} from '../personalization_state.js';

import {AVATAR_PLACEHOLDER_URL} from './utils.js';

/**
 * @fileoverview Utility functions to derive a user image URL to display from
 * |PersonalizationState|. Results are cached when necessary to avoid expensive
 * re-computation.
 */

/**
 * Transforming a |BigBuffer| to a blob url is expensive. Cache results by
 * reference to a |BigBuffer|. Use |WeakMap| so that old |BigBuffer| objects
 * that are no longer stored in |PersonalizationState| can be garbage collected.
 */
const objectUrlCache = new WeakMap<BigBuffer, Url>();

function bufferToPngObjectUrl(value: BigBuffer): Url|null {
  if (value.invalidBuffer) {
    console.error('Invalid buffer received');
    return null;
  }

  if (objectUrlCache.has(value)) {
    return objectUrlCache.get(value)!;
  }

  try {
    let bytes: Uint8Array;
    if (Array.isArray(value.bytes)) {
      bytes = new Uint8Array(value.bytes);
    } else {
      assert(!!value.sharedMemory, 'sharedMemory must be defined here');
      const sharedMemory = value.sharedMemory!;
      const {buffer, result} =
          sharedMemory.bufferHandle.mapBuffer(0, sharedMemory.size);
      assert(result === Mojo.RESULT_OK, 'Could not map buffer');
      bytes = new Uint8Array(buffer);
    }

    const result = {
      url: URL.createObjectURL(new Blob([bytes], {type: 'image/png'})),
    };
    objectUrlCache.set(value, result);
    return result;

  } catch (e) {
    console.error('Unable to create blob from image data', e);
    return null;
  }
}

/**
 * The placeholder url is used as the user image url for invalid or unknown
 * urls.
 */
const placeHolderUrl = {
  url: AVATAR_PLACEHOLDER_URL,
};

/**
 * Derive a user image |Url| from |PersonalizationState|. Return a |Url| rather
 * than |string| to avoid copies on potentially large data URLs.
 */
export function selectUserImageUrl(state: PersonalizationState): Url|null {
  const userImage = state.user.image;

  if (!userImage) {
    return null;
  }

  // Generated types for |userImage| are incorrect for mojom unions. Only one
  // key should be present at runtime. This weird construction allows typescript
  // to do exhaustiveness checking in the switch/case.
  assert(Object.keys(userImage).length === 1, 'only one key is set');
  const key = Object.keys(userImage)[0] as keyof UserImage;
  switch (key) {
    case 'invalidImage':
      return placeHolderUrl;
    case 'defaultImage':
      return userImage.defaultImage!.url;
    case 'profileImage':
      return state.user.profileImage;
    case 'externalImage':
      return bufferToPngObjectUrl(userImage.externalImage!);
    default:
      console.error('Unknown image type received', key);
      return placeHolderUrl;
  }
}

/**
 * Derive the |Url| to display of the last external user image. This is an image
 * from camera or file.
 */
export function selectLastExternalUserImageUrl(state: PersonalizationState):
    Url|null {
  const lastExternalUserImage = state.user.lastExternalUserImage;

  if (!lastExternalUserImage) {
    return null;
  }

  const buffer = lastExternalUserImage.externalImage;
  assert(!!buffer, 'externalImage must be set');
  return bufferToPngObjectUrl(buffer);
}
