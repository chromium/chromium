// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';

export const SUBDOMAIN_SPECIFIER = '*.';

/**
 * Returns a favicon url for a given site.
 */
export function getFaviconUrl(site: string): string {
  // Use 'http' as the scheme if `site` has a wildcard scheme.
  let faviconUrl =
      site.startsWith('*://') ? site.replace('*://', 'http://') : site;

  // if `site` ends in a wildcard path, trim it.
  if (faviconUrl.endsWith('/*')) {
    faviconUrl = faviconUrl.substring(0, faviconUrl.length - 2);
  }

  return getFaviconForPageURL(
      faviconUrl, /*isSyncedUrlForHistoryUi=*/ false,
      /*remoteIconUrlForUma=*/ '', /*size=*/ 20);
}

/**
 * Returns if the given site matches all of its subdomains.
 */
export function matchesSubdomains(site: string): boolean {
  // Sites that match all subdomains for a given host will specify "*.<host>".
  // Given how sites are specified as origins for user specified sites and how
  // extension host permissions are specified, it should be safe to assume
  // that "*." will only be used to match subdomains.
  return site.includes(SUBDOMAIN_SPECIFIER);
}
