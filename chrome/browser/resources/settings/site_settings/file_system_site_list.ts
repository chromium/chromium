// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-list' is an element representing a list of origin-specific
 * permission entries for the File System Access API.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import './file_system_site_entry.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin} from '../router.js';

import {ContentSettingsTypes} from './constants.js';
import {getTemplate} from './file_system_site_list.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

export interface FileSystemGrant {
  isDirectory: boolean;
  displayName: string;  // Might be a shortened file path.
  filePath: string;
}

export interface OriginFileSystemGrants {
  origin: string;
  viewGrants: FileSystemGrant[];
  editGrants: FileSystemGrant[];
}

declare global {
  interface HTMLElementEventMap {
    'revoke-grants': CustomEvent<OriginFileSystemGrants>;
  }
}

const FileSystemSiteListElementBase =
    WebUiListenerMixin(RouteObserverMixin(SiteSettingsMixin(PolymerElement)));

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

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingChooserPermissionChanged',
        (category: ContentSettingsTypes) => {
          if (category === ContentSettingsTypes.FILE_SYSTEM_WRITE) {
            this.populateList_();
          }
        });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
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
    const response = await this.browserProxy.getFileSystemGrants();
    this.set('allowedGrants_', response);
  }

  /**
   * Determines whether there are any allowed File System Access permission
   * grants.
   */
  private hasAllowedGrants_(): boolean {
    return this.allowedGrants_.length > 0;
  }

  /**
   * Revoke all permission grants for a given origin, then update the list
   * displayed on the UI.
   */
  private onRevokeGrants_(e: CustomEvent<OriginFileSystemGrants>) {
    this.browserProxy.revokeFileSystemGrants(e.detail.origin);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-list': FileSystemSiteListElement;
  }
}

customElements.define(FileSystemSiteListElement.is, FileSystemSiteListElement);
