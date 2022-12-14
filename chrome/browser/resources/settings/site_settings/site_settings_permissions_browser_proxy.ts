// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the "Site Settings" to interact with
 * the permission-related updates of the browser.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {ContentSettingsTypes} from './constants.js';
// clang-format on

export interface UnusedSitePermissions {
  origin: string;
  permissions: ContentSettingsTypes[];
}

/**
 * TODO(crbug.com/1383197): Move functions related to notification permission
 * review here as well.
 */
export interface SiteSettingsPermissionsBrowserProxy {
  /**
   * Mark revoked permissions of unused sites as reviewed by the user so they
   * will not be shown again.
   */
  acknowledgeRevokedUnusedSitePermissionsList(
      unusedSitePermissionsList: UnusedSitePermissions[]): void;

  /**
   * Allow permissions again for an unused site where permissions were
   * auto-revoked. The origin will not appear again for the user to review and
   * permissions will not be auto-revoked for this origin in the future.
   */
  allowPermissionsAgainForUnusedSite(unusedSitePermissions:
                                         UnusedSitePermissions): void;

  /**
   * Gets the unused origins along with the permissions they have been granted.
   */
  getRevokedUnusedSitePermissionsList(): Promise<UnusedSitePermissions[]>;
}

export class SiteSettingsPermissionsBrowserProxyImpl implements
    SiteSettingsPermissionsBrowserProxy {
  acknowledgeRevokedUnusedSitePermissionsList(unusedSitePermissionsList:
                                                  UnusedSitePermissions[]) {
    chrome.send(
        'acknowledgeRevokedUnusedSitePermissionsList',
        [unusedSitePermissionsList]);
  }

  allowPermissionsAgainForUnusedSite(unusedSitePermissions:
                                         UnusedSitePermissions) {
    chrome.send('allowPermissionsAgainForUnusedSite', [unusedSitePermissions]);
  }

  getRevokedUnusedSitePermissionsList() {
    return sendWithPromise('getRevokedUnusedSitePermissionsList');
  }

  static getInstance(): SiteSettingsPermissionsBrowserProxy {
    return instance ||
        (instance = new SiteSettingsPermissionsBrowserProxyImpl());
  }

  static setInstance(obj: SiteSettingsPermissionsBrowserProxy) {
    instance = obj;
  }
}

let instance: SiteSettingsPermissionsBrowserProxy|null = null;
