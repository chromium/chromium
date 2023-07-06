// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-list' is an element representing a list of origin-specific
 * permission entries for the File System Access API.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import './file_system_site_entry.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Route, RouteObserverMixin, Router} from '../router.js';

import {ContentSettingsTypes} from './constants.js';
import {getTemplate} from './file_system_site_list.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

export interface FileSystemGrant {
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

declare global {
  interface HTMLElementEventMap {
    'options-icon-click': CustomEvent<OriginFileSystemGrants>;
    'revoke-grant': CustomEvent<FileSystemGrant>;
  }
}

export interface FileSystemSiteListElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
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

      /**
       * String representing the selected origin that has permissions granted
       * via the File System Access API.
       */
      selectedOrigin_: String,
    };
  }

  private allowedGrants_: OriginFileSystemGrants[];
  private selectedOrigin_: string;

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

  private onOpenOptionsMenu_(e: CustomEvent<OriginFileSystemGrants>) {
    this.selectedOrigin_ = e.detail.origin;
    this.$.menu.get().showAt(e.target as HTMLElement);
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
   * Revoke an individual permission grant for a given origin and filePath,
   * then update the list displayed on the UI.
   */
  private onRevokeGrant_(e: CustomEvent<FileSystemGrant>) {
    this.browserProxy.revokeFileSystemGrant(e.detail.origin, e.detail.filePath);
  }

  /**
   * Revoke all permission grants for a given origin, then update the list
   * displayed on the UI.
   */
  private onRemoveGrantsClick_() {
    this.browserProxy.revokeFileSystemGrants(this.selectedOrigin_);
    this.$.menu.get().close();
    this.selectedOrigin_ = '';
  }

  /**
   * Navigate to the Site Details page for a given origin.
   */
  private onViewSiteDetailsClick_() {
    this.$.menu.get().close();
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + this.selectedOrigin_));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-list': FileSystemSiteListElement;
  }
}

customElements.define(FileSystemSiteListElement.is, FileSystemSiteListElement);
