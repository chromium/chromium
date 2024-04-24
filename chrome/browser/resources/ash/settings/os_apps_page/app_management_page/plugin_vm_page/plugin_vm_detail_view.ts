// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../pin_to_shelf_item.js';
import '../../../app_management_icons.html.js';
import '../permission_item.js';
import '../app_management_cros_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast} from '../../../assert_extras.js';
import {AppManagementStoreMixin} from '../../../common/app_management/store_mixin.js';
import {PrefsState} from '../../../common/types.js';
import {Router, routes} from '../../../router.js';
import {AppManagementPermissionItemElement} from '../permission_item.js';

import {PluginVmBrowserProxy, PluginVmBrowserProxyImpl} from './plugin_vm_browser_proxy.js';
import {getTemplate} from './plugin_vm_detail_view.html.js';

const AppManagementPluginVmDetailViewElementBase =
    AppManagementStoreMixin(WebUiListenerMixin(PolymerElement));

export class AppManagementPluginVmDetailViewElement extends
    AppManagementPluginVmDetailViewElementBase {
  static get is() {
    return 'app-management-plugin-vm-detail-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      app_: Object,

      showDialog_: {
        type: Boolean,
        value: false,
      },

      dialogText_: String,

      pendingPermissionItem_: Object,
    };
  }

  prefs: PrefsState;
  private app_: App;
  private dialogText_: string;
  private pendingPermissionItem_: AppManagementPermissionItemElement;
  private pluginVmBrowserProxy_: PluginVmBrowserProxy;
  private showDialog_: boolean;

  constructor() {
    super();

    this.pluginVmBrowserProxy_ = PluginVmBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    // When the state is changed, get the new selected app and assign it to
    // |app_|
    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  }

  private onSharedPathsClick_(): void {
    Router.getInstance().navigateTo(
        routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS,
        new URLSearchParams({id: this.app_.id}));
  }

  private onSharedUsbDevicesClick_(): void {
    Router.getInstance().navigateTo(
        routes.APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES,
        new URLSearchParams({id: this.app_.id}));
  }

  private async onPermissionChanged_(e: Event): Promise<void> {
    this.pendingPermissionItem_ =
        cast(e.target, AppManagementPermissionItemElement);
    switch (this.pendingPermissionItem_.permissionType) {
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

    const requiresRelaunch =
        await this.pluginVmBrowserProxy_.isRelaunchNeededForNewPermissions();
    if (requiresRelaunch) {
      this.showDialog_ = true;
    } else {
      this.pendingPermissionItem_.syncPermission();
    }
  }

  private onRelaunchClick_(): void {
    this.pendingPermissionItem_.syncPermission();
    this.pluginVmBrowserProxy_.relaunchPluginVm();
    this.showDialog_ = false;
  }

  private onCancel_(): void {
    this.pendingPermissionItem_.resetToggle();
    this.showDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-plugin-vm-detail-view':
        AppManagementPluginVmDetailViewElement;
  }
}

customElements.define(
    AppManagementPluginVmDetailViewElement.is,
    AppManagementPluginVmDetailViewElement);
