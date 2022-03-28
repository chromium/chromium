// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_details_item.js';
import '//resources/cr_components/app_management/more_permissions_item.js';
import './pin_to_shelf_item.js';
import './shared_style.js';

import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';

import {BrowserProxy} from './browser_proxy.js';
import {AppManagementStoreClient} from './store_client.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-chrome-app-detail-view',

  behaviors: [
    AppManagementStoreClient,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: {
      type: Object,
      observer: 'onAppChanged_',
    },

    /**
     * @private {Array<ExtensionAppPermissionMessage>}
     */
    messages_: Object,
  },

  attached() {
    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @private
   */
  onAppChanged_: async function() {
    try {
      const {messages: messages} =
          await BrowserProxy.getInstance()
              .handler.getExtensionAppPermissionMessages(this.app_.id);
      this.messages_ = messages;
    } catch (err) {
      console.warn(err);
    }
  },

  /**
   * @param {!Array<ExtensionAppPermissionMessage>} messages
   * @return {Array<string>}
   * @private
   */
  getPermissionMessages_(messages) {
    return messages.map(m => m.message);
  },

  /**
   * @param {number} index
   * @param {!Array<ExtensionAppPermissionMessage>} messages
   * @return {?Array<string>}
   * @private
   */
  getPermissionSubmessagesByMessage_(index, messages) {
    // Dom-repeat still tries to access messages[0] when app has no
    // permission therefore we add an extra check.
    if (!messages[index]) {
      return null;
    }
    return messages[index].submessages;
  },

  /**
   * @param {!Array<ExtensionAppPermissionMessage>} messages
   * @return {boolean}
   * @private
   */
  hasPermissions_(messages) {
    return messages.length > 0;
  },
});
