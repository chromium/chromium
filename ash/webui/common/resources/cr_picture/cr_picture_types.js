// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const CrPicture = {};

/**
 * Contains the possible types for picture list image elements.
 * @enum {string}
 */
CrPicture.SelectionTypes = {
  CAMERA: 'camera',
  FILE: 'file',
  PROFILE: 'profile',
  // 'old' image is any selected non-profile and non-default image. It can be
  // from the camera or a file.
  OLD: 'old',
  DEFAULT: 'default',
  // 'deprecated' image is from the deprecated set of default images which is
  // still selected by the user.
  DEPRECATED: 'deprecated',
  NONE: '',
};

/**
 * An picture list image element.
 * @typedef {{
 *   dataset: {
 *     type: !CrPicture.SelectionTypes,
 *     index: (number|undefined),
 *     imageIndex: (number|undefined),
 *   },
 *   src: string,
 * }}
 */
CrPicture.ImageElement;

CrPicture.kDefaultImageUrl = 'chrome://theme/IDR_LOGIN_DEFAULT_USER';
