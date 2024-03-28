// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-details' shows the individual permission grant details
 * for permissions granted via the File System Access API, under Site Settings.
 */
import './file_system_site_entry_item.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';

import {ContentSettingsTypes} from './constants.js';
import {getTemplate} from './file_system_site_details.html.js';
import type {FileSystemGrant, OriginFileSystemGrants} from './file_system_site_list.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

declare global {
  interface HTMLElementEventMap {
    'revoke-grant': CustomEvent<FileSystemGrant>;
  }
}

const FileSystemSiteDetailsElementBase = WebUiListenerMixin(BaseMixin(
    RouteObserverMixin(SiteSettingsMixin(I18nMixin(PolymerElement)))));

export class FileSystemSiteDetailsElement extends
    FileSystemSiteDetailsElementBase {
  static get is() {
    return 'file-system-site-details';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Use the string representing the origin or extension name as the page
       * title of the settings-subpage parent.
       */
      pageTitle: {
        type: String,
        notify: true,
      },

      /**
       * The origin that this details page is showing information for.
       */
      origin_: String,

      /**
       * An Object representing an origin and its associated permission grants.
       */
      grantsPerOrigin: Object,
    };
  }
  pageTitle: string;
  private origin_: string;
  grantsPerOrigin: OriginFileSystemGrants;

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
   * RouteObserverMixin
   */
  override currentRouteChanged(route: Route) {
    if (route !== routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS) {
      return;
    }
    const site = Router.getInstance().getQueryParameters().get('site');
    if (!site) {
      return;
    }
    this.browserProxy.isOriginValid(site).then(valid => {
      if (!valid) {
        Router.getInstance().navigateToPreviousRoute();
      }
      this.origin_ = site;
      this.pageTitle = this.origin_;
    });
    this.populateList_();
  }

  /**
   * Retrieves a list of all known origins with allowed permissions,
   * granted via the File System Access API.
   */
  private async populateList_() {
    const response = await this.browserProxy.getFileSystemGrants();
    const originFileSystemGrantsObj =
        response.find(grantObj => grantObj.origin === this.origin_);
    // Return to the file system site settings page if the given origin has
    // no file system grants.
    if (!originFileSystemGrantsObj) {
      Router.getInstance().navigateTo(routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
      return;
    }
    this.grantsPerOrigin = originFileSystemGrantsObj;
  }

  /**
   * Revoke an individual permission grant for a given origin and filePath,
   * then update the list displayed on the UI.
   */
  private onRevokeGrant_(e: CustomEvent<FileSystemGrant>) {
    this.browserProxy.revokeFileSystemGrant(this.origin_, e.detail.filePath);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-details': FileSystemSiteDetailsElement;
  }
}

customElements.define(
    FileSystemSiteDetailsElement.is, FileSystemSiteDetailsElement);
