// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the "Site Settings" to interact with
 * the permission-related updates of the browser.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {ContentSettingsTypes} from '../site_settings/constants.js';
// clang-format on

export interface UnusedSitePermissions {
  origin: string;
  permissions: ContentSettingsTypes[];
  expiration: string;
}

/**
 * TODO(crbug.com/1383197): Move functions related to notification permission
 * review here as well.
 */
export interface SafetyHubBrowserProxy {
  /**
   * Mark revoked permissions of unused sites as reviewed by the user so they
   * will not be shown again.
   */
  acknowledgeRevokedUnusedSitePermissionsList(): void;

  /**
   * Allow permissions again for an unused site where permissions were
   * auto-revoked. The origin will not appear again for the user to review and
   * permissions will not be auto-revoked for this origin in the future.
   */
  allowPermissionsAgainForUnusedSite(origin: string): void;

  /**
   * Gets the unused origins along with the permissions they have been granted.
   */
  getRevokedUnusedSitePermissionsList(): Promise<UnusedSitePermissions[]>;

  /**
   * Reverse the changes made by |acknowledgeRevokedUnusedSitePermissionsList|.
   * The list of sites will be presented again to the user for review.
   */
  undoAcknowledgeRevokedUnusedSitePermissionsList(
      unusedSitePermissionsList: UnusedSitePermissions[]): void;

  /**
   * Reverse the changes made by |allowPermissionsAgainForUnusedSite|. This will
   * revoke the origin's permissions, re-enable auto-revocation for this origin,
   * and the entry will be visible again in the UI.
   */
  undoAllowPermissionsAgainForUnusedSite(unusedSitePermissions:
                                             UnusedSitePermissions): void;
}

export class SafetyHubBrowserProxyImpl implements SafetyHubBrowserProxy {
  acknowledgeRevokedUnusedSitePermissionsList() {
    chrome.send('acknowledgeRevokedUnusedSitePermissionsList');
  }

  allowPermissionsAgainForUnusedSite(origin: string) {
    chrome.send('allowPermissionsAgainForUnusedSite', [origin]);
  }

  getRevokedUnusedSitePermissionsList() {
    return sendWithPromise('getRevokedUnusedSitePermissionsList');
  }

  undoAcknowledgeRevokedUnusedSitePermissionsList(unusedSitePermissionsList:
                                                      UnusedSitePermissions[]) {
    chrome.send(
        'undoAcknowledgeRevokedUnusedSitePermissionsList',
        [unusedSitePermissionsList]);
  }

  undoAllowPermissionsAgainForUnusedSite(unusedSitePermissions:
                                             UnusedSitePermissions) {
    chrome.send(
        'undoAllowPermissionsAgainForUnusedSite', [unusedSitePermissions]);
  }

  static getInstance(): SafetyHubBrowserProxy {
    return instance || (instance = new SafetyHubBrowserProxyImpl());
  }

  static setInstance(obj: SafetyHubBrowserProxy) {
    instance = obj;
  }
}

let instance: SafetyHubBrowserProxy|null = null;
