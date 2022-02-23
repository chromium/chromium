// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_details_item.js';
import './pin_to_shelf_item.js';
import './resize_lock_item.js';
import './supported_links_item.js';
import './shared_style.js';
import '//resources/cr_components/app_management/icons.js';
import '//resources/cr_components/app_management/more_permissions_item.js';
import '//resources/cr_components/app_management/permission_item.js';
import '//resources/cr_elements/icons.m.js';

import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getAppIcon, getPermission, getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';

import {BrowserProxy} from './browser_proxy.js';
import {AppManagementStoreClient} from './store_client.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-arc-detail-view',

  behaviors: [
    AppManagementStoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,

    /**
     * @private {boolean}
     */
    listExpanded_: {
      type: Boolean,
      value: false,
    },
  },

  attached() {
    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();

    this.listExpanded_ = false;
  },

  /**
   * @private
   */
  toggleListExpanded_() {
    this.listExpanded_ = !this.listExpanded_;
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_(app) {
    return getAppIcon(app);
  },

  /**
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  getCollapsedIcon_(listExpanded) {
    return listExpanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * Returns true if the app has not requested any permissions.
   *
   * @param {App} app
   * @return {boolean}
   * @private
   */
  noPermissionsRequested_(app) {
    const permissionItems =
        this.$$('#subpermission-list')
            .querySelectorAll('app-management-permission-item');
    for (let i = 0; i < permissionItems.length; i++) {
      const permissionItem = permissionItems[i];
      const permission = getPermission(app, permissionItem.permissionType);
      if (permission !== undefined) {
        return false;
      }
    }
    return true;
  },
});
