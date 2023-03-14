// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-list' is an element representing a list of origin-specific
 * permission entries for the File System Access API.
 */

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Route, RouteObserverMixin, Router} from '../router.js';

import {getTemplate} from './file_system_site_list.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {RawFileSystemGrant} from './site_settings_prefs_browser_proxy.js';

interface FileSystemGrant {
  isDirectory: boolean;
  displayName: string;  // Might be a shortened file path
  origin: string;
  filePath: string;
}

export interface OriginFileSystemGrants {
  origin: string;
  viewGrants: FileSystemGrant[];
  editGrants: FileSystemGrant[];
}

/**
 * Map the given edit grants to the allowPermissionGrantsList, to be displayed
 * on the UI.
 *
 * @param grants A list of file system permission grants returned from
 *     the browserProxy.
 * @param isDirectory Determines whether the grants being mapped over
 *     are for a file or for a directory.
 * @returns A list of allowed view grants, where there is not an existing
 *     write grant for the given origin / filepath.
 */
function mapEditGrantsToAllowPermissionGrants(
    grants: RawFileSystemGrant[], isDirectory: boolean): FileSystemGrant[] {
  return grants.map(grant => {
    return {
      origin: grant.origin,
      isDirectory: isDirectory,
      filePath: grant.filePath,
      displayName: grant.filePath,
    };
  });
}

/**
 * Filter out view grants that have a corresponding write grant,
 * so that there is only either a view OR edit grant displayed on the
 * chrome://settings/content/filesystem UI for a given origin + filepath.
 *
 * Map the filtered view grants to the allowPermissionGrantsList,
 * to be displayed on the UI.
 *
 * @param grants A list of file system permission grants returned from
 *     the browserProxy.
 * @param allowPermissionGrantsList A list of formatted permission grants, to
 *     be displayed on the UI.
 * @param isDirectory Determines whether the grants being mapped over
 *     are for a file or for a directory.
 * @returns A list of allowed view grants, where there is not an existing
 *     write grant for the given origin / filepath.
 */
function mapViewGrantsToAllowPermissionGrants(
    viewGrants: RawFileSystemGrant[], editGrants: FileSystemGrant[],
    isDirectory: boolean): FileSystemGrant[] {
  return viewGrants
      .filter(
          viewGrant => !editGrants.some(
              editGrant => editGrant.filePath === viewGrant.filePath))
      .map(viewGrant => {
        return {
          origin: viewGrant.origin,
          isDirectory: isDirectory,
          filePath: viewGrant.filePath,
          displayName: viewGrant.filePath,
        };
      });
}

const FileSystemSiteListElementBase =
    RouteObserverMixin(SiteSettingsMixin(PolymerElement));

export class FileSystemSiteListElement extends FileSystemSiteListElementBase {
  static get is() {
    return 'file-system-site-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Array of the File System permission grants that are actively displayed,
       * grouped by origin.
       */
      allowedGrants_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private allowedGrants_: OriginFileSystemGrants[];
  private eventTracker_: EventTracker;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_ = new EventTracker();

    /**
     * Update the allowed permission grants list when the UI is visible.
     */
    this.eventTracker_.add(document, 'visibilitychange', () => {
      const currentRoute = Router.getInstance().currentRoute;
      if (document.visibilityState === 'visible' &&
          currentRoute === routes.SITE_SETTINGS_FILE_SYSTEM_WRITE) {
        this.populateList_();
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  /**
   * Reload the site list when the chrome://settings/content/filesystem
   * page is visited.
   *
   * RouteObserverMixin
   */
  override currentRouteChanged(currentRoute: Route, oldRoute?: Route) {
    if (currentRoute === routes.SITE_SETTINGS_FILE_SYSTEM_WRITE &&
        currentRoute !== oldRoute) {
      this.populateList_();
    }
  }

  /**
   * Retrieves a list of all known origins with allowed permissions,
   * granted via the File System Access API.
   */
  private async populateList_() {
    // TODO(crbug.com/1373962): Display |response| in the UI once mocks are
    // done.
    const response = await this.browserProxy.getFileSystemGrants();
    this.allowedGrants_ = [];

    for (let i = 0; i < response.length; i++) {
      const allowPermissionGrants: OriginFileSystemGrants = {
        origin: '',
        viewGrants: [],
        editGrants: [],
      };
      allowPermissionGrants.origin = response[i].origin;

      allowPermissionGrants.editGrants = mapEditGrantsToAllowPermissionGrants(
          response[i].fileWriteGrants, false);

      allowPermissionGrants.editGrants =
          allowPermissionGrants.editGrants.concat(
              mapEditGrantsToAllowPermissionGrants(
                  response[i].directoryWriteGrants, true));

      allowPermissionGrants.viewGrants = mapViewGrantsToAllowPermissionGrants(
          response[i].fileReadGrants, allowPermissionGrants.editGrants, false);

      allowPermissionGrants.viewGrants =
          allowPermissionGrants.viewGrants.concat(
              mapViewGrantsToAllowPermissionGrants(
                  response[i].directoryReadGrants,
                  allowPermissionGrants.editGrants, true));

      // TODO(crbug.com/1373962): Look into whether this logic can be
      // simplified before the launch of the Persistent Permissions
      // settings page UI.
      const existingIndex = this.allowedGrants_.findIndex(
          grant => grant.origin === response[i].origin);
      if (existingIndex !== -1) {
        this.set(`allowedGrants_.${existingIndex}`, allowPermissionGrants);
      } else {
        this.push('allowedGrants_', allowPermissionGrants);
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-list': FileSystemSiteListElement;
  }
}

customElements.define(FileSystemSiteListElement.is, FileSystemSiteListElement);
