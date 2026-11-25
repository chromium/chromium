// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'file-system-site-details' shows the individual permission grant details
 * for permissions granted via the File System Access API, under Site Settings.
 */
import './file_system_site_entry_item.js';
import '../settings_page/settings_subpage.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';
import type {Route} from '../router.js';
import {Router} from '../router.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {ContentSettingsTypes} from './constants.js';
import {getCss} from './file_system_site_details.css.js';
import {getHtml} from './file_system_site_details.html.js';
import type {FileSystemGrant, OriginFileSystemGrants} from './file_system_site_list.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

declare global {
  interface HTMLElementEventMap {
    'revoke-grant': CustomEvent<FileSystemGrant>;
  }
}

const FileSystemSiteDetailsElementBase = WebUiListenerMixinLit(
    SettingsViewMixinLit(SiteSettingsMixinLit(CrLitElement)));

export class FileSystemSiteDetailsElement extends
    FileSystemSiteDetailsElementBase {
  static get is() {
    return 'file-system-site-details';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Use the string representing the origin or extension name as the page
       * title of the settings-subpage parent.
       */
      pageTitle_: {type: String},

      /**
       * An Object representing an origin and its associated permission grants.
       */
      grantsPerOrigin_: {type: Object},
    };
  }

  protected accessor pageTitle_: string = '';
  protected accessor grantsPerOrigin_: OriginFileSystemGrants = {
    origin: '',
    viewGrants: [],
    editGrants: [],
  };

  /**
   * The origin that this details page is showing information for.
   */
  private origin_: string = '';

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'contentSettingChooserPermissionChanged',
        (category: ContentSettingsTypes) => {
          if (category === ContentSettingsTypes.FILE_SYSTEM_WRITE &&
              this.origin_) {
            this.populateList_();
          }
        });
  }

  /**
   * SettingsViewMixinLit
   */
  override currentRouteChanged(route: Route, oldRoute?: Route) {
    super.currentRouteChanged(route, oldRoute);

    this.origin_ = '';
    this.grantsPerOrigin_ = {
      origin: '',
      viewGrants: [],
      editGrants: [],
    };
    if (route !== routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS) {
      return;
    }
    const site = Router.getInstance().getQueryParameters().get('site');
    if (!site) {
      return;
    }
    this.browserProxy.isOriginValid(site).then(valid => {
      if (Router.getInstance().getCurrentRoute() !==
              routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS ||
          Router.getInstance().getQueryParameters().get('site') !== site) {
        return;
      }
      if (!valid) {
        Router.getInstance().navigateToPreviousRoute();
        return;
      }
      this.origin_ = site;
      this.pageTitle_ = this.origin_;
      this.populateList_();
    });
  }

  /**
   * Retrieves a list of all known origins with allowed permissions,
   * granted via the File System Access API.
   */
  private async populateList_() {
    const targetOrigin = this.origin_;
    if (!targetOrigin) {
      return;
    }
    const response = await this.browserProxy.getFileSystemGrants();
    if (this.origin_ !== targetOrigin) {
      return;
    }
    const originFileSystemGrantsObj =
        response.find(grantObj => grantObj.origin === targetOrigin);
    // Return to the file system site settings page if the given origin has
    // no file system grants.
    if (!originFileSystemGrantsObj) {
      Router.getInstance().navigateTo(routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
      return;
    }
    this.grantsPerOrigin_ = originFileSystemGrantsObj;
  }

  /**
   * Revoke an individual permission grant for a given origin and filePath,
   * then update the list displayed on the UI.
   */
  protected onRevokeGrant_(e: CustomEvent<FileSystemGrant>) {
    this.browserProxy.revokeFileSystemGrant(this.origin_, e.detail.filePath);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'file-system-site-details': FileSystemSiteDetailsElement;
  }
}

customElements.define(
    FileSystemSiteDetailsElement.is, FileSystemSiteDetailsElement);
