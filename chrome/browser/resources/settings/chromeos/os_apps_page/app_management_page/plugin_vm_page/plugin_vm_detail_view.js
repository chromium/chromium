// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../pin_to_shelf_item.js';
import '//resources/cr_components/app_management/icons.js';
import '//resources/cr_components/app_management/permission_item.js';
import '../shared_style.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/icons.m.js';

import {assertNotReached} from '//resources/js/assert.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';

import {Router} from '../../../../router.js';
import {routes} from '../../../os_route.js';
import {AppManagementStoreClient} from '../store_client.js';

import {PluginVmBrowserProxyImpl} from './plugin_vm_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-plugin-vm-detail-view',

  behaviors: [
    AppManagementStoreClient,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * @private {App}
     */
    app_: Object,

    /** @private {boolean} */
    showDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    dialogText_: String,

    /** @private {Element} */
    pendingPermissionItem_: Object,
  },

  attached() {
    // When the state is changed, get the new selected app and assign it to
    // |app_|
    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  },

  /** @private */
  onSharedPathsClick_() {
    Router.getInstance().navigateTo(
        routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS,
        new URLSearchParams({'id': this.app_.id}));
  },

  /** @private */
  onSharedUsbDevicesClick_() {
    Router.getInstance().navigateTo(
        routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES,
        new URLSearchParams({'id': this.app_.id}));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onPermissionChanged_: async function(e) {
    this.pendingPermissionItem_ =
        /** @type {AppManamentPermissionItemElement} */ (e.target);
    switch (e.target.permissionType) {
      case 'kCamera':
        this.dialogText_ =
            loadTimeData.getString('pluginVmPermissionDialogCameraLabel');
        break;
      case 'kMicrophone':
        this.dialogText_ =
            loadTimeData.getString('pluginVmPermissionDialogMicrophoneLabel');
        break;
      default:
        assertNotReached();
    }

    const requiresRelaunch = await PluginVmBrowserProxyImpl.getInstance()
                                 .isRelaunchNeededForNewPermissions();
    if (requiresRelaunch) {
      this.showDialog_ = true;
    } else {
      this.pendingPermissionItem_.syncPermission();
    }
  },

  onRelaunchTap_: function() {
    this.pendingPermissionItem_.syncPermission();
    PluginVmBrowserProxyImpl.getInstance().relaunchPluginVm();
    this.showDialog_ = false;
  },

  onCancel_: function() {
    this.pendingPermissionItem_.resetToggle();
    this.showDialog_ = false;
  },
});
