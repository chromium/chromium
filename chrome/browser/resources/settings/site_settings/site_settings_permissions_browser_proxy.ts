// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the "Site Settings" to interact with
 * the permission-related updates of the browser.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface UnusedSitePermissions {
  origin: string;
  permissions: string[];
}

/**
 * TODO(crbug.com/1383197): Move functions related to notification permission
 * review here as well.
 */
export interface SiteSettingsPermissionsBrowserProxy {
  /**
   * Gets the unused origins along with the permissions they have been granted.
   */
  getRevokedUnusedSitePermissionsList(): Promise<UnusedSitePermissions[]>;
}

export class SiteSettingsPermissionsBrowserProxyImpl implements
    SiteSettingsPermissionsBrowserProxy {
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
