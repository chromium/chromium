// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be used for avatars.
 */

/** The placeholder url for an avatar, rendered if the avatar url is invalid. */
export const AVATAR_PLACEHOLDER_URL: string =
    'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';

/**
 * Returns the avatar url. If necessary, prefixes the url with the sanitizing
 * string.
 */
export function getAvatarUrl(
    url: string, staticEncode: boolean = false): string {
  if (!url) {
    return '';
  }
  if (url.startsWith('data') || url.startsWith('blob') ||
      url === AVATAR_PLACEHOLDER_URL) {
    return url;
  }
  if (!staticEncode) {
    return `chrome://image/?${url}`;
  }
  return `chrome://image/?url=${encodeURIComponent(url)}&staticEncode=true`;
}
