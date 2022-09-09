// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';

/**
 * Returns a favicon url for a given site.
 */
export function getFaviconUrl(site: string, size: number = 20): string {
  // Use 'http' as the scheme if `site` has a wildcard scheme.
  let faviconUrl =
      site.startsWith('*://') ? site.replace('*://', 'http://') : site;

  // if `site` ends in a wildcard path, trim it.
  if (faviconUrl.endsWith('/*')) {
    faviconUrl = faviconUrl.substring(0, faviconUrl.length - 2);
  }

  return getFaviconForPageURL(
      faviconUrl, /*isSyncedUrlForHistoryUi=*/ false,
      /*remoteIconUrlForUma=*/ '', size);
}
