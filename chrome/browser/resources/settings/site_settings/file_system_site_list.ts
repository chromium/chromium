// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-list' is an element representing a list of origin-specific
 * permission entries for the File System Access API.
 */
import './file_system_site_entry.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit} from '../router.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {ContentSettingsTypes} from './constants.js';
import {getHtml} from './file_system_site_list.html.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

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

const FileSystemSiteListElementBase = WebUiListenerMixinLit(
    RouteObserverMixinLit(SiteSettingsMixinLit(CrLitElement)));

export class FileSystemSiteListElement extends FileSystemSiteListElementBase {
  static get is() {
    return 'file-system-site-list';
  }

  static override get styles() {
    return getSettingsSharedCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Array of the File System permission grants that are actively displayed,
       * grouped by origin.
       */
      allowedGrants_: {type: Array},
    };
  }

  protected accessor allowedGrants_: OriginFileSystemGrants[] = [];

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

  /**
   * Reload the site list when the chrome://settings/content/filesystem
   * page is visited.
   *
   * RouteObserverMixinLit
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
    this.allowedGrants_ = response;
  }

  /**
   * Determines whether there are any allowed File System Access permission
   * grants.
   */
  protected hasAllowedGrants_(): boolean {
    return this.allowedGrants_.length > 0;
  }

  /**
   * Revoke all permission grants for a given origin, then update the list
   * displayed on the UI.
   */
  protected onRevokeGrants_(e: CustomEvent<OriginFileSystemGrants>) {
    this.browserProxy.revokeFileSystemGrants(e.detail.origin);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-list': FileSystemSiteListElement;
  }
}

customElements.define(FileSystemSiteListElement.is, FileSystemSiteListElement);
